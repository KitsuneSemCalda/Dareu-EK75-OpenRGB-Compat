#pragma once
#include <array>
#include <chrono>
#include <map>
#include <vector>

/*---------------------------------------------------------*\
| A software model of the 2.4G receiver and the keyboard    |
| behind it, implementing the hidapi calls the driver makes.|
| Its behaviour follows docs/RESEARCH.md:                   |
|                                                           |
|  - target 0 reaches the receiver, (slot + 1) << 4 the     |
|    keyboard; a packet for any other target gets no reply  |
|  - replies are asynchronous: GET_FEATURE reports "not     |
|    ready" a few times before the answer shows up          |
|  - commands sent too close together are dropped           |
|  - a LED_CMD_FRAME wedges the receiver for good           |
\*---------------------------------------------------------*/
typedef std::array<unsigned char, 64> FakePacket;

struct FakeRegion
{
    unsigned char               type        = 4;
    unsigned char               fps         = 0;
    unsigned char               rows        = 0;
    unsigned char               columns     = 0;
    std::vector<unsigned char>  effects;

    unsigned char               effect      = 1;
    unsigned char               flag        = 0;
    unsigned char               speed       = 1;
    std::vector<unsigned char>  rgb;            /* 3 bytes per colour */
    unsigned char               brightness  = 255;
};

struct FakeReceiver
{
    /*-----------------------------------------------------*\
    | Configuration, set by the test                        |
    \*-----------------------------------------------------*/
    bool                        keyboard_paired = true;
    unsigned int                slot            = 0;
    unsigned short              keyboard_pid    = 0x0045;
    unsigned int                not_ready_polls = 1;
    bool                        answer_wireless = true;
    std::map<int, FakeRegion>   regions;

    /*-----------------------------------------------------*\
    | Observations, read by the test                        |
    \*-----------------------------------------------------*/
    std::vector<FakePacket>     sent;
    unsigned int                dropped         = 0;
    unsigned int                frame_packets   = 0;
    bool                        wedged          = false;
    unsigned int                handles_closed  = 0;

    /*-----------------------------------------------------*\
    | Internals                                             |
    \*-----------------------------------------------------*/
    FakePacket                  reply{};
    bool                        reply_pending   = false;
    unsigned int                polls_left      = 0;
    std::chrono::steady_clock::time_point last_command;
    bool                        have_last       = false;

    /* Defaults matching the real unit: keys and side light */
    void Reset();

    unsigned char               KeyboardTarget() const { return (unsigned char)((slot + 1) << 4); }
};

extern FakeReceiver g_receiver;

/*---------------------------------------------------------*\
| The receiver drops commands closer together than this. The|
| real limit is not known. It is above the 10 ms reply poll |
| delay, so a driver without its own gap gets dropped, and  |
| below the driver's 20 ms.                                 |
\*---------------------------------------------------------*/
#define FAKE_MIN_COMMAND_GAP_MS 15
