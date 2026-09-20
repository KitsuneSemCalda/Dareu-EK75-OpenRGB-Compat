/*---------------------------------------------------------*\
| Integration tests: the detector, the controllers and the  |
| OpenRGB-facing modes, talking to the fake firmware        |
\*---------------------------------------------------------*/
#include "cest.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "DetectionManager.h"
#include "DareuEK75Controller.h"
#include "RGBController_DareuEK75.h"
#include "helpers.h"
#include "LogManager.h"

DetectedControllers DetectDareuEK75(hid_device_info* info, const std::string& name);

static void Fresh()
{
    g_receiver.Reset();
}

static DetectedControllers Detect()
{
    hid_device_info info;
    char path[] = "/dev/hidraw9";
    info.path = path;

    return DetectDareuEK75(&info, "Dareu EK75");
}

static void Free(DetectedControllers& list)
{
    for(RGBController* c : list)
    {
        delete c;
    }

    list.clear();
}

static int FindMode(RGBController* c, const std::string& name)
{
    for(unsigned int i = 0; i < c->modes.size(); i++)
    {
        if(c->modes[i].name == name)
        {
            return (int)i;
        }
    }

    return -1;
}

static mode& Mode(RGBController* c, const std::string& name)
{
    return c->modes[FindMode(c, name)];
}

static bool Logged(const std::string& needle)
{
    for(const std::string& line : g_test_log)
    {
        if(line.find(needle) != std::string::npos)
        {
            return true;
        }
    }

    return false;
}

/*---------------------------------------------------------*\
| Effects last written to the firmware for a region         |
\*---------------------------------------------------------*/
static FakeRegion& Fw(int region)
{
    return g_receiver.regions[region];
}

static bool FwColor(int region, unsigned char r, unsigned char g, unsigned char b)
{
    const std::vector<unsigned char>& rgb = Fw(region).rgb;

    return rgb.size() == 3 && rgb[0] == r && rgb[1] == g && rgb[2] == b;
}

int main(int argc, char* argv[])
{
    cest_init(argc, argv);

    describe("Detector registration", {
        it("registers for the receiver, interface 3", {
            const std::vector<TestHIDDetector>& list = TestHIDDetectors();

            expect((int)list.size()).toEqual(1);
            expect(list[0].name).toEqual("Dareu EK75");
            expect(I(list[0].vid >> 8)).toEqual(0x26);
            expect(I(list[0].vid & 0xFF)).toEqual(0x0D);
            expect(I(list[0].pid)).toEqual(0x42);
            expect(list[0].interface_number).toEqual(3);
        });
    });

    describe("Detection", {
        beforeEach(Fresh);

        it("creates one device per region, named from the detector", {
            DetectedControllers found = Detect();

            expect((int)found.size()).toEqual(2);
            expect(found[0]->name).toEqual("Dareu EK75");
            expect(found[1]->name).toEqual("Dareu EK75 Side Light");
            Free(found);
        });

        it("types the keys as a keyboard and the light bar as a strip", {
            DetectedControllers found = Detect();

            expect(found[0]->type).toEqual(DEVICE_TYPE_KEYBOARD);
            expect(found[1]->type).toEqual(DEVICE_TYPE_LEDSTRIP);
            expect(found[0]->vendor).toEqual("Dareu");
            Free(found);
        });

        it("reports the HID path and region as location", {
            DetectedControllers found = Detect();

            expect(found[0]->location).toEqual("HID: /dev/hidraw9 region 1");
            expect(found[1]->location).toEqual("HID: /dev/hidraw9 region 4");
            Free(found);
        });

        it("finds nothing and says why when no keyboard is paired", {
            g_receiver.keyboard_paired = false;
            DetectedControllers found = Detect();

            expect((int)found.size()).toEqual(0);
            expect(Logged("switched to 2.4G")).toBeTruthy();
        });

        it("skips a region the firmware does not report", {
            g_receiver.regions.erase(4);
            DetectedControllers found = Detect();

            expect((int)found.size()).toEqual(1);
            expect(found[0]->name).toEqual("Dareu EK75");
            Free(found);
        });

        it("keeps the HID handle until the last region is gone", {
            DetectedControllers found = Detect();

            delete found[0];
            expect(I(g_receiver.handles_closed)).toEqual(0);
            delete found[1];
            expect(I(g_receiver.handles_closed)).toEqual(1);
        });

        it("has a single LED per region, as the firmware takes one colour", {
            DetectedControllers found = Detect();

            expect((int)found[0]->zones.size()).toEqual(1);
            expect((int)found[0]->leds.size()).toEqual(1);
            expect(found[0]->zones[0].name).toEqual("Keyboard");
            expect(found[1]->zones[0].name).toEqual("Side Light");
            Free(found);
        });
    });

    describe("Modes of the key matrix", {
        beforeEach(Fresh);

        it("lists Direct first, the firmware effects by id, and Off last", {
            DetectedControllers found = Detect();
            RGBController* keys = found[0];

            expect((int)keys->modes.size()).toEqual(23);
            expect(keys->modes.front().name).toEqual("Direct");
            expect(keys->modes.front().value).toEqual(0x100);
            expect(keys->modes[1].name).toEqual("Static");
            expect(keys->modes[2].name).toEqual("Breathing");
            expect(keys->modes.back().name).toEqual("Off");
            expect(keys->modes.back().value).toEqual(0x101);
            Free(found);
        });

        it("hides the streaming effect", {
            DetectedControllers found = Detect();

            expect(FindMode(found[0], "Streaming Frame")).toEqual(-1);
            for(const mode& m : found[0]->modes)
            {
                expect(m.value == 18).toBeFalsy();
            }
            Free(found);
        });

        it("skips an effect id it has no name for", {
            g_receiver.regions[1].effects.push_back(99);
            DetectedControllers found = Detect();

            expect((int)found[0]->modes.size()).toEqual(23);
            Free(found);
        });

        it("gives Static one colour and no speed", {
            DetectedControllers found = Detect();
            mode& m = Mode(found[0], "Static");

            expect(I(m.color_mode)).toEqual(I(MODE_COLORS_MODE_SPECIFIC));
            expect(I(m.colors_max)).toEqual(1);
            expect((m.flags & MODE_FLAG_HAS_SPEED) != 0).toBeFalsy();
            expect((m.flags & MODE_FLAG_HAS_RANDOM_COLOR) != 0).toBeFalsy();
            Free(found);
        });

        it("gives Breathing up to five colours, a speed of 1 to 3 and random colour", {
            DetectedControllers found = Detect();
            mode& m = Mode(found[0], "Breathing");

            expect(I(m.colors_max)).toEqual(5);
            expect(I(m.speed_min)).toEqual(1);
            expect(I(m.speed_max)).toEqual(3);
            expect((m.flags & MODE_FLAG_HAS_RANDOM_COLOR) != 0).toBeTruthy();
            expect((m.flags & MODE_FLAG_HAS_BRIGHTNESS) != 0).toBeTruthy();
            Free(found);
        });

        it("gives Starlit two colours", {
            DetectedControllers found = Detect();

            expect(I(Mode(found[0], "Starlit").colors_max)).toEqual(2);
            Free(found);
        });

        it("gives Neon a speed but no colour", {
            DetectedControllers found = Detect();
            mode& m = Mode(found[0], "Neon");

            expect(I(m.color_mode)).toEqual(I(MODE_COLORS_NONE));
            expect((m.flags & MODE_FLAG_HAS_SPEED) != 0).toBeTruthy();
            Free(found);
        });

        it("makes Direct per-LED with brightness, and Off colourless", {
            DetectedControllers found = Detect();
            mode& direct = Mode(found[0], "Direct");
            mode& off = Mode(found[0], "Off");

            expect(I(direct.color_mode)).toEqual(I(MODE_COLORS_PER_LED));
            expect((direct.flags & MODE_FLAG_HAS_BRIGHTNESS) != 0).toBeTruthy();
            expect(I(off.color_mode)).toEqual(I(MODE_COLORS_NONE));
            expect((off.flags & MODE_FLAG_HAS_BRIGHTNESS) != 0).toBeFalsy();
            Free(found);
        });
    });

    describe("Modes of the side light", {
        beforeEach(Fresh);

        it("has no Direct and uses the firmware's own Off", {
            DetectedControllers found = Detect();
            RGBController* side = found[1];

            expect(FindMode(side, "Direct")).toEqual(-1);
            expect((int)side->modes.size()).toEqual(3);
            expect(side->modes[0].name).toEqual("Off");
            expect(side->modes[0].value).toEqual(0);
            expect(side->modes[1].name).toEqual("Static");
            expect(side->modes[2].name).toEqual("Breathing");
            Free(found);
        });

        it("offers no colour for Static, which ignores it on region 4", {
            DetectedControllers found = Detect();

            expect(I(Mode(found[1], "Static").color_mode)).toEqual(I(MODE_COLORS_NONE));
            expect(I(Mode(found[1], "Breathing").color_mode)).toEqual(I(MODE_COLORS_MODE_SPECIFIC));
            Free(found);
        });
    });

    describe("Reading the keyboard state at load", {
        beforeEach(Fresh);

        it("selects the mode the keyboard is in, with its speed, colour and brightness", {
            g_receiver.regions[1].effect = 5;
            g_receiver.regions[1].speed = 3;
            g_receiver.regions[1].rgb = Bytes({ 0, 255, 0 });
            g_receiver.regions[1].brightness = 120;

            DetectedControllers found = Detect();
            RGBController* keys = found[0];

            expect(keys->modes[keys->active_mode].name).toEqual("Wave");
            expect(I(Mode(keys, "Wave").speed)).toEqual(3);
            expect(Mode(keys, "Wave").colors[0] == ToRGBColor(0, 255, 0)).toBeTruthy();
            expect(keys->colors[0] == ToRGBColor(0, 255, 0)).toBeTruthy();
            expect(I(Mode(keys, "Wave").brightness)).toEqual(120);
            Free(found);
        });

        it("marks random colour when the effect has an empty colour list", {
            g_receiver.regions[1].effect = 2;
            g_receiver.regions[1].rgb.clear();

            DetectedControllers found = Detect();

            expect(I(Mode(found[0], "Breathing").color_mode)).toEqual(I(MODE_COLORS_RANDOM));
            Free(found);
        });

        it("only reads, so loading does not change the lighting", {
            DetectedControllers found = Detect();

            expect(I(CountSets())).toEqual(0);
            Free(found);
        });

        it("keeps the default mode for an effect it has no mode for", {
            g_receiver.regions[1].effect = 17;
            DetectedControllers found = Detect();

            expect(I(found[0]->active_mode)).toEqual(0);
            expect(Logged("has no mode")).toBeTruthy();
            Free(found);
        });

        it("reads the side light on its own region", {
            g_receiver.regions[4].effect = 2;
            g_receiver.regions[4].brightness = 70;
            DetectedControllers found = Detect();

            expect(found[1]->modes[found[1]->active_mode].name).toEqual("Breathing");
            expect(I(Mode(found[1], "Breathing").brightness)).toEqual(70);
            Free(found);
        });
    });

    describe("Applying modes", {
        beforeEach(Fresh);

        it("writes an animated effect with its speed and colours", {
            DetectedControllers found = Detect();
            RGBController* keys = found[0];
            mode& wave = Mode(keys, "Wave");

            wave.speed = 2;
            wave.colors[0] = ToRGBColor(1, 2, 3);
            keys->SetActiveMode(FindMode(keys, "Wave"));

            expect(I(Fw(1).effect)).toEqual(5);
            expect(I(Fw(1).speed)).toEqual(2);
            expect(FwColor(1, 1, 2, 3)).toBeTruthy();
            Free(found);
        });

        it("writes several colours for Breathing", {
            DetectedControllers found = Detect();
            RGBController* keys = found[0];
            mode& m = Mode(keys, "Breathing");

            m.colors.push_back(ToRGBColor(9, 9, 9));
            keys->SetActiveMode(FindMode(keys, "Breathing"));

            expect((int)Fw(1).rgb.size()).toEqual(6);
            Free(found);
        });

        it("sends no colour for random colour, so the effect cycles the rainbow", {
            DetectedControllers found = Detect();
            RGBController* keys = found[0];

            Mode(keys, "Wave").color_mode = MODE_COLORS_RANDOM;
            keys->SetActiveMode(FindMode(keys, "Wave"));

            expect(I(Fw(1).effect)).toEqual(5);
            expect((int)Fw(1).rgb.size()).toEqual(0);
            Free(found);
        });

        it("sends no colour for Neon", {
            DetectedControllers found = Detect();

            found[0]->SetActiveMode(FindMode(found[0], "Neon"));

            expect(I(Fw(1).effect)).toEqual(3);
            expect((int)Fw(1).rgb.size()).toEqual(0);
            Free(found);
        });

        it("emulates Off on the keys as Static in black", {
            DetectedControllers found = Detect();

            found[0]->SetActiveMode(FindMode(found[0], "Off"));

            expect(I(Fw(1).effect)).toEqual(1);
            expect(FwColor(1, 0, 0, 0)).toBeTruthy();
            Free(found);
        });

        it("uses the firmware's Off effect on the side light", {
            DetectedControllers found = Detect();

            found[1]->SetActiveMode(FindMode(found[1], "Off"));

            expect(I(Fw(4).effect)).toEqual(0);
            expect(I(Fw(1).effect)).toEqual(1);
            Free(found);
        });

        it("writes only its own region", {
            DetectedControllers found = Detect();
            g_receiver.regions[1].effect = 5;

            found[1]->SetActiveMode(FindMode(found[1], "Breathing"));

            expect(I(Fw(4).effect)).toEqual(2);
            expect(I(Fw(1).effect)).toEqual(5);
            Free(found);
        });

        it("sends brightness only when it changed", {
            DetectedControllers found = Detect();
            RGBController* keys = found[0];

            Forget();
            keys->SetActiveMode(FindMode(keys, "Wave"));
            unsigned int without = (unsigned int)g_receiver.sent.size();

            Forget();
            Mode(keys, "Wave").brightness = 100;
            keys->SetActiveMode(FindMode(keys, "Wave"));
            unsigned int with = (unsigned int)g_receiver.sent.size();

            expect(I(without)).toEqual(1);
            expect(I(with)).toEqual(2);
            expect(I(Fw(1).brightness)).toEqual(100);

            Forget();
            keys->SetActiveMode(FindMode(keys, "Wave"));
            expect((int)g_receiver.sent.size()).toEqual(1);
            Free(found);
        });
    });

    describe("Direct mode", {
        beforeEach(Fresh);

        it("paints the whole region with the LED colour", {
            DetectedControllers found = Detect();
            RGBController* keys = found[0];

            keys->colors[0] = ToRGBColor(0, 0, 255);
            keys->SetActiveMode(FindMode(keys, "Direct"));

            expect(I(Fw(1).effect)).toEqual(1);
            expect(FwColor(1, 0, 0, 255)).toBeTruthy();
            Free(found);
        });

        it("follows colour changes", {
            DetectedControllers found = Detect();
            RGBController* keys = found[0];

            keys->colors[0] = ToRGBColor(0, 0, 255);
            keys->SetActiveMode(FindMode(keys, "Direct"));
            keys->colors[0] = ToRGBColor(255, 128, 0);
            keys->UpdateLEDs();

            expect(FwColor(1, 255, 128, 0)).toBeTruthy();
            Free(found);
        });

        it("does not resend a colour it already sent", {
            DetectedControllers found = Detect();
            RGBController* keys = found[0];

            keys->colors[0] = ToRGBColor(0, 0, 255);
            keys->SetActiveMode(FindMode(keys, "Direct"));
            Forget();

            for(int i = 0; i < 5; i++)
            {
                keys->UpdateLEDs();
                keys->DeviceUpdateZoneLEDs(0);
                keys->DeviceUpdateSingleLED(0);
            }

            expect((int)g_receiver.sent.size()).toEqual(0);
            Free(found);
        });

        it("sends one effect for one colour change, not one per hook", {
            DetectedControllers found = Detect();
            RGBController* keys = found[0];

            keys->colors[0] = ToRGBColor(0, 0, 255);
            keys->SetActiveMode(FindMode(keys, "Direct"));
            Forget();

            keys->colors[0] = ToRGBColor(9, 9, 9);
            keys->UpdateLEDs();

            expect(I(CountSets())).toEqual(1);
            Free(found);
        });

        it("sends a colour again when the first attempt got no reply", {
            DetectedControllers found = Detect();
            RGBController* keys = found[0];

            keys->SetActiveMode(FindMode(keys, "Direct"));

            /* the keyboard stops answering: its target no longer matches the slot */
            g_receiver.slot = 1;
            keys->colors[0] = ToRGBColor(0, 0, 255);
            keys->UpdateLEDs();
            expect(FwColor(1, 0, 0, 255)).toBeFalsy();

            g_receiver.slot = 0;
            Forget();
            keys->UpdateLEDs();

            expect(I(CountSets())).toEqual(1);
            expect(FwColor(1, 0, 0, 255)).toBeTruthy();
            Free(found);
        });

        it("is ignored by other modes when the LED colour changes", {
            DetectedControllers found = Detect();
            RGBController* keys = found[0];

            keys->SetActiveMode(FindMode(keys, "Wave"));
            Forget();
            keys->colors[0] = ToRGBColor(9, 9, 9);
            keys->UpdateLEDs();

            expect((int)g_receiver.sent.size()).toEqual(0);
            Free(found);
        });
    });

    describe("Safety", {
        beforeEach(Fresh);

        it("never sends a frame command, whatever the client does", {
            DetectedControllers found = Detect();

            for(RGBController* c : found)
            {
                for(unsigned int i = 0; i < c->modes.size(); i++)
                {
                    c->colors[0] = ToRGBColor(i, 2 * i, 3 * i);
                    c->SetActiveMode(i);
                    c->UpdateLEDs();
                }
            }

            expect(I(g_receiver.frame_packets)).toEqual(0);
            expect(g_receiver.wedged).toBeFalsy();
            expect(I(g_receiver.dropped)).toEqual(0);
            Free(found);
        });
    });

    return cest_result();
}
