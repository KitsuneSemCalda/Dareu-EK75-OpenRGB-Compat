#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "fake_receiver.h"
#include "hidapi.h"
#include "LogManager.h"
#include "DetectionManager.h"

FakeReceiver g_receiver;

std::vector<std::string> g_test_log;

std::vector<TestHIDDetector>& TestHIDDetectors()
{
    static std::vector<TestHIDDetector> detectors;
    return detectors;
}

void LogAppend(int level, const char* fmt, ...)
{
    char text[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    g_test_log.push_back(std::to_string(level) + " " + text);
}

void FakeReceiver::Reset()
{
    *this = FakeReceiver();

    FakeRegion keys;
    keys.fps        = 33;
    keys.rows       = 6;
    keys.columns    = 15;
    keys.effects    = { 1, 2, 5, 11, 4, 9, 6, 3, 10, 20, 21, 22, 24, 25, 26, 27, 28, 29, 30, 31, 32, 18 };
    keys.effect     = 1;
    keys.rgb        = { 255, 0, 0 };

    FakeRegion side;
    side.rows       = 1;
    side.columns    = 16;
    side.effects    = { 0, 2, 1, 18 };
    side.effect     = 1;

    regions[1]      = keys;
    regions[4]      = side;

    g_test_log.clear();
}

/*---------------------------------------------------------*\
| Build the answer for one command. reply[6..] is payload   |
\*---------------------------------------------------------*/
static void Answer(FakeReceiver& r, const unsigned char* p)
{
    const unsigned char target  = p[0];
    const unsigned char cls     = p[2];
    const unsigned char cmd     = p[3];
    const bool          get     = (cmd & 0x80) != 0;
    const unsigned char op      = cmd & 0x7F;
    const unsigned char* pl     = &p[6];

    FakePacket out{};
    out[0] = (unsigned char)((target & 0xF0) | 0x02);
    out[2] = cls;
    out[3] = cmd;
    out[4] = p[4];

    if(cls == 0)
    {
        /* A wired keyboard has no receiver to ask, so this command never gets a reply. */
        if(r.wired || target != 0 || op != 32 || !get || !r.answer_wireless)
        {
            return;
        }

        /*-------------------------------------------------*\
        | Slot count, then status, PID high, PID low per     |
        | slot. Slots before the paired one are empty.       |
        \*-------------------------------------------------*/
        out[1] = 7;
        out[6] = (unsigned char)(r.slot + 1);

        unsigned char* entry = &out[7 + (3 * r.slot)];

        entry[0] = r.keyboard_paired ? 1 : 0;
        entry[1] = (unsigned char)(r.keyboard_pid >> 8);
        entry[2] = (unsigned char)(r.keyboard_pid & 0xFF);
    }
    else if(cls == 3)
    {
        /* Wired: the keyboard itself answers at target 0, no pairing to check. */
        const unsigned char expected_target = r.wired ? 0 : r.KeyboardTarget();

        if(target != expected_target || (!r.wired && !r.keyboard_paired))
        {
            return;
        }

        const int region = pl[0];

        if(!r.regions.count(region))
        {
            return;
        }

        FakeRegion& reg = r.regions[region];

        out[6] = (unsigned char)region;

        if(op == 1 && get)
        {
            out[7]  = reg.type;
            out[8]  = reg.fps;
            out[9]  = reg.rows;
            out[10] = reg.columns;
            out[11] = (unsigned char)reg.effects.size();

            for(size_t i = 0; i < reg.effects.size(); i++)
            {
                out[12 + i] = reg.effects[i];
            }
        }
        else if(op == 2)
        {
            if(!get)
            {
                reg.effect  = pl[1];
                reg.flag    = pl[2];
                reg.speed   = pl[3];
                reg.rgb.assign(&pl[5], &pl[5] + (3 * pl[4]));
            }

            out[7]  = reg.effect;
            out[8]  = reg.flag;
            out[9]  = reg.speed;
            out[10] = (unsigned char)(reg.rgb.size() / 3);

            for(size_t i = 0; i < reg.rgb.size(); i++)
            {
                out[11 + i] = reg.rgb[i];
            }
        }
        else if(op == 3)
        {
            if(!get)
            {
                reg.brightness = pl[1];
            }

            out[7] = reg.brightness;
        }
        else
        {
            return;
        }
    }
    else
    {
        return;
    }

    r.reply         = out;
    r.reply_pending = true;
    r.polls_left    = r.not_ready_polls;
}

extern "C"
{
hid_device* hid_open_path(const char* /*path*/)
{
    return (hid_device*)&g_receiver;
}

void hid_close(hid_device* /*dev*/)
{
    g_receiver.handles_closed++;
}

int hid_send_feature_report(hid_device* /*dev*/, const unsigned char* data, size_t length)
{
    if(length != 65 || data[0] != 0)
    {
        return -1;
    }

    FakeReceiver& r = g_receiver;

    FakePacket packet;
    memcpy(packet.data(), &data[1], 64);
    r.sent.push_back(packet);

    if(r.wedged)
    {
        return -1;
    }

    if(packet[2] == 3 && (packet[3] & 0x7F) == 4)
    {
        r.frame_packets++;
        r.wedged = true;
        return -1;
    }

    auto now = std::chrono::steady_clock::now();

    if(r.have_last && std::chrono::duration_cast<std::chrono::milliseconds>(now - r.last_command).count() < FAKE_MIN_COMMAND_GAP_MS)
    {
        r.dropped++;
        r.last_command = now;
        return (int)length;
    }

    r.last_command  = now;
    r.have_last     = true;
    r.reply_pending = false;

    Answer(r, packet.data());

    return (int)length;
}

int hid_get_feature_report(hid_device* /*dev*/, unsigned char* data, size_t length)
{
    FakeReceiver& r = g_receiver;

    if(length != 65 || r.wedged)
    {
        return -1;
    }

    memset(data, 0, length);

    if(!r.reply_pending)
    {
        return (int)length;
    }

    if(r.polls_left > 0)
    {
        r.polls_left--;
        memcpy(&data[1], r.reply.data(), 64);
        data[1] &= 0xF0;
        return (int)length;
    }

    memcpy(&data[1], r.reply.data(), 64);

    return (int)length;
}
}
