/*---------------------------------------------------------*\
| Unit tests: packet encoding, reply decoding, addressing   |
| and the rules of DareuEK75Device::Transfer()              |
\*---------------------------------------------------------*/
#include "cest.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/*---------------------------------------------------------*\
| Transfer() is private on purpose, and the frame refusal   |
| cannot be reached through the public API                  |
\*---------------------------------------------------------*/
#define private public
#include "DareuEK75Controller.h"
#undef private

#include "helpers.h"
#include "LogManager.h"

static void Fresh()
{
    g_receiver.Reset();
}

static std::shared_ptr<DareuEK75Device> Connected()
{
    auto dev = std::make_shared<DareuEK75Device>((hid_device*)&g_receiver, "/dev/hidraw9");

    dev->Connect();
    Forget();

    return dev;
}

static bool HasLog(const std::string& needle)
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

int main(int argc, char* argv[])
{
    cest_init(argc, argv);

    describe("Connect", {
        beforeEach(Fresh);

        it("addresses the receiver itself with a wireless status request", {
            DareuEK75Device dev((hid_device*)&g_receiver, "p");
            dev.Connect();

            FakePacket p = g_receiver.sent.at(0);
            expect(I(p[0])).toEqual(0x00);
            expect(I(p[1])).toEqual(7);
            expect(I(p[2])).toEqual(0);
            expect(I(p[3])).toEqual(0xA0);
            expect(I(p[4])).toEqual(0);
        });

        it("targets slot 0 as 0x10 and reads the keyboard PID", {
            DareuEK75Device dev((hid_device*)&g_receiver, "p");

            expect(dev.Connect()).toBeTruthy();
            expect(I(dev.target_id)).toEqual(0x10);
            expect(I(dev.GetKeyboardPID())).toEqual(0x0045);
        });

        it("targets slot 1 as 0x20", {
            g_receiver.slot = 1;
            DareuEK75Device dev((hid_device*)&g_receiver, "p");

            expect(dev.Connect()).toBeTruthy();
            expect(I(dev.target_id)).toEqual(0x20);
        });

        it("fails when no keyboard is paired", {
            g_receiver.keyboard_paired = false;
            DareuEK75Device dev((hid_device*)&g_receiver, "p");

            expect(dev.Connect()).toBeFalsy();
            expect(I(dev.target_id)).toEqual(0);
        });

        it("fails when the receiver never answers", {
            g_receiver.answer_wireless = false;
            DareuEK75Device dev((hid_device*)&g_receiver, "p");

            expect(dev.Connect()).toBeFalsy();
        });

        it("reports its location with the HID prefix", {
            DareuEK75Device dev((hid_device*)&g_receiver, "/dev/hidraw9");

            expect(dev.GetLocation()).toEqual("HID: /dev/hidraw9");
        });

        it("closes the HID handle when destroyed", {
            {
                DareuEK75Device dev((hid_device*)&g_receiver, "p");
            }

            expect(I(g_receiver.handles_closed)).toEqual(1);
        });
    });

    describe("Packet encoding", {
        beforeEach(Fresh);

        it("encodes a brightness read", {
            auto dev = Connected();
            unsigned char level;
            dev->GetBrightness(4, level);

            FakePacket p = g_receiver.sent.back();
            expect(I(p[0])).toEqual(0x10);
            expect(I(p[1])).toEqual(2);
            expect(I(p[2])).toEqual(3);
            expect(I(p[3])).toEqual(0x83);
            expect(I(p[4])).toEqual(1);
            expect(I(p[5])).toEqual(0);
            expect(I(p[6])).toEqual(4);
        });

        it("encodes a brightness write", {
            auto dev = Connected();
            dev->SetBrightness(1, 120);

            FakePacket p = g_receiver.sent.back();
            expect(I(p[3])).toEqual(0x03);
            expect(I(p[6])).toEqual(1);
            expect(I(p[7])).toEqual(120);
        });

        it("encodes an attribute read on profile 0", {
            auto dev = Connected();
            DareuRegionInfo info;
            dev->GetRegionInfo(1, info);

            FakePacket p = g_receiver.sent.back();
            expect(I(p[3])).toEqual(0x81);
            expect(I(p[4])).toEqual(0);
        });

        it("encodes an effect write with size 5 + 3 per colour", {
            auto dev = Connected();
            DareuEffectState s;
            s.effect = 2;
            s.flag = 0;
            s.speed = 3;
            s.colors = Cols({ ToRGBColor(1, 2, 3), ToRGBColor(4, 5, 6) });
            dev->SetEffect(1, s);

            FakePacket p = g_receiver.sent.back();
            expect(I(p[0])).toEqual(0x10);
            expect(I(p[1])).toEqual(11);
            expect(I(p[2])).toEqual(3);
            expect(I(p[3])).toEqual(0x02);
            expect(I(p[4])).toEqual(1);
            expect(I(p[6])).toEqual(1);
            expect(I(p[7])).toEqual(2);
            expect(I(p[9])).toEqual(3);
            expect(I(p[10])).toEqual(2);
            expect(I(p[11])).toEqual(1);
            expect(I(p[12])).toEqual(2);
            expect(I(p[13])).toEqual(3);
            expect(I(p[14])).toEqual(4);
            expect(I(p[16])).toEqual(6);
        });

        it("encodes an effect without colours as size 5", {
            auto dev = Connected();
            DareuEffectState s;
            s.effect = 3;
            dev->SetEffect(1, s);

            FakePacket p = g_receiver.sent.back();
            expect(I(p[1])).toEqual(5);
            expect(I(p[10])).toEqual(0);
        });

        it("sends at most 5 colours", {
            auto dev = Connected();
            DareuEffectState s;
            s.effect = 2;

            for(int i = 0; i < 8; i++)
            {
                s.colors.push_back(ToRGBColor(i, i, i));
            }

            dev->SetEffect(1, s);

            FakePacket p = g_receiver.sent.back();
            expect(I(p[1])).toEqual(20);
            expect(I(p[10])).toEqual(5);
        });
    });

    describe("Reply decoding", {
        beforeEach(Fresh);

        it("reads the layout of the key matrix", {
            auto dev = Connected();
            DareuRegionInfo info;

            expect(dev->GetRegionInfo(1, info)).toBeTruthy();
            expect(I(info.region)).toEqual(1);
            expect(I(info.type)).toEqual(4);
            expect(I(info.fps)).toEqual(33);
            expect(I(info.rows)).toEqual(6);
            expect(I(info.columns)).toEqual(15);
            expect((int)info.effects.size()).toEqual(22);
            expect(I(info.effects[0])).toEqual(1);
        });

        it("reads the effects of the side light", {
            auto dev = Connected();
            DareuRegionInfo info;

            expect(dev->GetRegionInfo(4, info)).toBeTruthy();
            expect((int)info.effects.size()).toEqual(4);
            expect(I(info.effects[0])).toEqual(0);
            expect(I(info.effects[3])).toEqual(18);
        });

        it("fails for a region the firmware does not have", {
            auto dev = Connected();
            DareuRegionInfo info;

            expect(dev->GetRegionInfo(9, info)).toBeFalsy();
        });

        it("reads effect, speed, direction and colours", {
            g_receiver.regions[1].effect = 5;
            g_receiver.regions[1].flag = 1;
            g_receiver.regions[1].speed = 3;
            g_receiver.regions[1].rgb = Bytes({ 10, 20, 30, 40, 50, 60 });

            auto dev = Connected();
            DareuEffectState s;

            expect(dev->GetEffect(1, s)).toBeTruthy();
            expect(I(s.effect)).toEqual(5);
            expect(I(s.flag)).toEqual(1);
            expect(I(s.speed)).toEqual(3);
            expect((int)s.colors.size()).toEqual(2);
            expect(I(RGBGetRValue(s.colors[0]))).toEqual(10);
            expect(I(RGBGetBValue(s.colors[1]))).toEqual(60);
        });

        it("rejects a reply with more colours than the protocol allows", {
            g_receiver.regions[1].rgb.assign(18, 7);

            auto dev = Connected();
            DareuEffectState s;

            expect(dev->GetEffect(1, s)).toBeFalsy();
        });

        it("round-trips an effect through the firmware", {
            auto dev = Connected();
            DareuEffectState in;
            in.effect = 11;
            in.speed = 2;
            in.colors = { ToRGBColor(9, 8, 7) };

            expect(dev->SetEffect(1, in)).toBeTruthy();

            DareuEffectState out;
            expect(dev->GetEffect(1, out)).toBeTruthy();
            expect(I(out.effect)).toEqual(11);
            expect(I(out.speed)).toEqual(2);
            expect((int)out.colors.size()).toEqual(1);
            expect(out.colors[0] == ToRGBColor(9, 8, 7)).toBeTruthy();
        });

        it("round-trips brightness", {
            auto dev = Connected();
            unsigned char level = 0;

            expect(dev->SetBrightness(4, 70)).toBeTruthy();
            expect(dev->GetBrightness(4, level)).toBeTruthy();
            expect(I(level)).toEqual(70);
        });

        it("fails a write to a region the firmware does not have", {
            auto dev = Connected();
            DareuEffectState s;

            expect(dev->SetEffect(9, s)).toBeFalsy();
        });
    });

    describe("Transfer rules", {
        beforeEach(Fresh);

        it("refuses LED_CMD_FRAME and never puts it on the wire", {
            auto dev = Connected();
            unsigned char reply[64];

            expect(dev->Transfer(0x10, 6, 3, 4, 0, { 1, 0x80, 0, 0, 15 }, reply)).toBeFalsy();
            expect(I(g_receiver.frame_packets)).toEqual(0);
            expect((int)g_receiver.sent.size()).toEqual(0);
            expect(g_receiver.wedged).toBeFalsy();
        });

        it("refuses LED_CMD_FRAME sent as a read too", {
            auto dev = Connected();
            unsigned char reply[64];

            expect(dev->Transfer(0x10, 6, 3, 4 | 0x80, 0, {}, reply)).toBeFalsy();
            expect((int)g_receiver.sent.size()).toEqual(0);
        });

        it("logs the refusal as an error", {
            auto dev = Connected();
            unsigned char reply[64];
            dev->Transfer(0x10, 6, 3, 4, 0, {}, reply);

            expect(HasLog("Refusing to send LED_CMD_FRAME")).toBeTruthy();
        });

        it("does not treat command 4 of another class as a frame", {
            auto dev = Connected();
            unsigned char reply[64];

            dev->Transfer(0x10, 0, 7, 4, 0, {}, reply);
            expect((int)g_receiver.sent.size()).toEqual(1);
        });

        it("rejects a payload that does not fit after the header", {
            auto dev = Connected();
            unsigned char reply[64];
            std::vector<unsigned char> big(59, 1);

            expect(dev->Transfer(0x10, 59, 3, 2, 1, big, reply)).toBeFalsy();
            expect((int)g_receiver.sent.size()).toEqual(0);
        });

        it("accepts the largest payload that fits", {
            auto dev = Connected();
            unsigned char reply[64];
            std::vector<unsigned char> max(58, 1);
            max[0] = 1;

            dev->Transfer(0x10, 58, 3, 2, 1, max, reply);
            expect((int)g_receiver.sent.size()).toEqual(1);
        });

        it("waits for a reply that is not ready yet", {
            g_receiver.not_ready_polls = 5;
            auto dev = Connected();
            unsigned char level;

            expect(dev->GetBrightness(1, level)).toBeTruthy();
        });

        it("gives up after 20 polls", {
            g_receiver.not_ready_polls = 25;
            auto dev = Connected();
            unsigned char level;

            expect(dev->GetBrightness(1, level)).toBeFalsy();
        });

        it("is not answered when addressed to another target", {
            auto dev = Connected();
            unsigned char reply[64];

            expect(dev->Transfer(0x30, 2, 3, 0x83, 1, { 1 }, reply)).toBeFalsy();
        });

        it("keeps 20 ms between commands so the receiver drops none", {
            auto dev = Connected();
            unsigned char level;

            for(int i = 0; i < 6; i++)
            {
                dev->GetBrightness(1, level);
            }

            expect(I(g_receiver.dropped)).toEqual(0);
        });

        it("spends the gap and one poll between two commands", {
            g_receiver.not_ready_polls = 0;
            auto dev = Connected();
            unsigned char level;
            dev->GetBrightness(1, level);

            auto start = std::chrono::steady_clock::now();
            dev->GetBrightness(1, level);
            dev->GetBrightness(1, level);
            long ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

            expect(ms).toBeGreaterThan(55);
        });

        it("serialises transfers from several threads", {
            auto dev = Connected();
            std::vector<std::thread> threads;
            int ok = 0;
            std::mutex m;

            for(int t = 0; t < 4; t++)
            {
                threads.emplace_back([&]() {
                    unsigned char level;
                    bool r = dev->GetBrightness(1, level);
                    std::lock_guard<std::mutex> g(m);
                    ok += r ? 1 : 0;
                });
            }

            for(std::thread& t : threads)
            {
                t.join();
            }

            expect(ok).toEqual(4);
            expect(I(g_receiver.dropped)).toEqual(0);
        });
    });

    describe("Controller facade", {
        beforeEach(Fresh);

        it("fixes the region and reports name and location", {
            auto dev = Connected();
            DareuRegionInfo info;
            dev->GetRegionInfo(4, info);

            DareuEK75Controller c(dev, 4, info, "Dareu EK75 Side Light");

            expect(c.GetNameString()).toEqual("Dareu EK75 Side Light");
            expect(c.GetDeviceLocation()).toEqual("HID: /dev/hidraw9 region 4");
            expect(I(c.GetRegion())).toEqual(4);
        });

        it("forwards effect and brightness to its own region", {
            auto dev = Connected();
            DareuRegionInfo info;
            dev->GetRegionInfo(4, info);
            DareuEK75Controller c(dev, 4, info, "x");

            DareuEffectState s;
            s.effect = 2;
            s.colors = { ToRGBColor(1, 1, 1) };

            expect(c.SetEffect(s)).toBeTruthy();
            expect(c.SetBrightness(33)).toBeTruthy();
            expect(I(g_receiver.regions[4].effect)).toEqual(2);
            expect(I(g_receiver.regions[4].brightness)).toEqual(33);
            expect(I(g_receiver.regions[1].brightness)).toEqual(255);
        });
    });

    return cest_result();
}
