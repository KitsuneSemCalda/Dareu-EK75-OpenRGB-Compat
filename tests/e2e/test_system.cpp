/*---------------------------------------------------------*\
| End-to-end tests of the driver as OpenRGB uses it: detect, |
| change the lighting, restart, and survive a bad receiver.  |
| The hardware is the fake receiver, everything above it is  |
| the real code.                                             |
\*---------------------------------------------------------*/
#include "cest.h"

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "DetectionManager.h"
#include "DareuEK75Controller.h"
#include "RGBController_DareuEK75.h"
#include "helpers.h"

DetectedControllers DetectDareuEK75(hid_device_info* info, const std::string& name);

static void Fresh()
{
    g_receiver.Reset();
}

static DetectedControllers Start()
{
    hid_device_info info;
    char path[] = "/dev/hidraw9";
    info.path = path;

    return DetectDareuEK75(&info, "Dareu EK75");
}

static void Stop(DetectedControllers& list)
{
    for(RGBController* c : list)
    {
        delete c;
    }

    list.clear();
}

static int Find(RGBController* c, const std::string& name)
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

void run_system_tests()
{
    describe("A session", {
        beforeEach(Fresh);

        it("keeps what a client set across an OpenRGB restart", {
            DetectedControllers first = Start();
            RGBController* keys = first[0];
            RGBController* side = first[1];

            keys->modes[Find(keys, "Wave")].speed = 3;
            keys->modes[Find(keys, "Wave")].colors[0] = ToRGBColor(0, 200, 100);
            keys->modes[Find(keys, "Wave")].brightness = 90;
            keys->SetActiveMode(Find(keys, "Wave"));
            side->SetActiveMode(Find(side, "Breathing"));
            Stop(first);

            DetectedControllers second = Start();

            expect(second[0]->modes[second[0]->active_mode].name).toEqual("Wave");
            expect(I(second[0]->modes[second[0]->active_mode].speed)).toEqual(3);
            expect(I(second[0]->modes[second[0]->active_mode].brightness)).toEqual(90);
            expect(second[0]->colors[0] == ToRGBColor(0, 200, 100)).toBeTruthy();
            expect(second[1]->modes[second[1]->active_mode].name).toEqual("Breathing");
            Stop(second);
        });

        it("does not change the lighting just by starting", {
            g_receiver.regions[1].effect = 11;
            g_receiver.regions[1].rgb = Bytes({ 1, 2, 3, 4, 5, 6 });
            g_receiver.regions[1].brightness = 70;

            DetectedControllers list = Start();
            Stop(list);

            expect(I(CountSets())).toEqual(0);
            expect(I(g_receiver.regions[1].effect)).toEqual(11);
            expect(I(g_receiver.regions[1].brightness)).toEqual(70);
        });

        it("shows a Direct colour as Static after a restart", {
            DetectedControllers first = Start();

            first[0]->colors[0] = ToRGBColor(255, 0, 255);
            first[0]->SetActiveMode(Find(first[0], "Direct"));
            Stop(first);

            DetectedControllers second = Start();

            expect(second[0]->modes[second[0]->active_mode].name).toEqual("Static");
            expect(second[0]->colors[0] == ToRGBColor(255, 0, 255)).toBeTruthy();
            Stop(second);
        });

        it("turns the keys off and back on", {
            DetectedControllers list = Start();
            RGBController* keys = list[0];

            keys->colors[0] = ToRGBColor(0, 255, 0);
            keys->SetActiveMode(Find(keys, "Direct"));
            keys->SetActiveMode(Find(keys, "Off"));

            expect(I(g_receiver.regions[1].rgb[1])).toEqual(0);

            keys->SetActiveMode(Find(keys, "Direct"));
            expect(I(g_receiver.regions[1].rgb[1])).toEqual(255);
            Stop(list);
        });

        it("applies every mode of every region and reads each one back", {
            DetectedControllers list = Start();

            for(RGBController* c : list)
            {
                for(unsigned int i = 0; i < c->modes.size(); i++)
                {
                    c->colors[0] = ToRGBColor(10, 20, 30);
                    c->SetActiveMode(i);

                    int region = (c == list[0]) ? 1 : 4;
                    int value = c->modes[i].value;
                    int expected = (value == 0x100) ? 1 : (value == 0x101 ? 1 : value);

                    expect(I(g_receiver.regions[region].effect)).toEqual(expected);
                }
            }

            expect(I(g_receiver.frame_packets)).toEqual(0);
            expect(I(g_receiver.dropped)).toEqual(0);
            Stop(list);
        });

        it("serves both regions from two threads at once", {
            DetectedControllers list = Start();
            std::thread a([&]() { list[0]->SetActiveMode(Find(list[0], "Wave")); });
            std::thread b([&]() { list[1]->SetActiveMode(Find(list[1], "Breathing")); });

            a.join();
            b.join();

            expect(I(g_receiver.regions[1].effect)).toEqual(5);
            expect(I(g_receiver.regions[4].effect)).toEqual(2);
            expect(I(g_receiver.dropped)).toEqual(0);
            Stop(list);
        });
    });

    describe("A bad receiver", {
        beforeEach(Fresh);

        it("finds the keyboard once it wakes up", {
            g_receiver.keyboard_paired = false;
            DetectedControllers asleep = Start();

            expect((int)asleep.size()).toEqual(0);

            g_receiver.keyboard_paired = true;
            DetectedControllers awake = Start();

            expect((int)awake.size()).toEqual(2);
            Stop(awake);
        });

        it("does not hang or crash when the receiver stops answering", {
            DetectedControllers list = Start();

            g_receiver.wedged = true;
            list[0]->SetActiveMode(Find(list[0], "Wave"));
            list[1]->SetActiveMode(Find(list[1], "Breathing"));

            expect(I(g_receiver.regions[1].effect)).toEqual(1);
            expect(I(g_receiver.regions[4].effect)).toEqual(1);
            Stop(list);
        });

        it("finds nothing on a receiver that stays wedged", {
            g_receiver.wedged = true;
            DetectedControllers list = Start();

            expect((int)list.size()).toEqual(0);
        });

        it("is not sent a frame even after a failed write", {
            DetectedControllers list = Start();

            g_receiver.not_ready_polls = 30;
            list[0]->SetActiveMode(Find(list[0], "Wave"));
            g_receiver.not_ready_polls = 1;
            list[0]->SetActiveMode(Find(list[0], "Wave"));

            expect(I(g_receiver.frame_packets)).toEqual(0);
            expect(I(g_receiver.regions[1].effect)).toEqual(5);
            Stop(list);
        });
    });
}
