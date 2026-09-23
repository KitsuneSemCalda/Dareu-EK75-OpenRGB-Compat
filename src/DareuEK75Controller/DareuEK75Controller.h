/*---------------------------------------------------------*\
| DareuEK75Controller.h                                     |
|                                                           |
|   Driver for the Dareu EK75 keyboard (TK51G), through its |
|   2.4G receiver or a direct USB connection                |
|                                                           |
|   Protocol reverse engineered by the open-ek75 project    |
|   (https://github.com/mateusands/open-ek75), see          |
|   docs/RESEARCH.md for what was verified on hardware      |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <hidapi.h>
#include "RGBController.h"

/*---------------------------------------------------------*\
| USB IDs                                                   |
|                                                             |
| Two transports, same vendor interface number and protocol |
| (verified [hw], see docs/RESEARCH.md#direct-usb-wired):    |
|   - through the 2.4G receiver, PID 0x0042                  |
|   - direct USB cable, PID 0x0045                           |
\*---------------------------------------------------------*/
#define DAREU_VID                       0x260D
#define DAREU_EK75_RECEIVER_PID         0x0042
#define DAREU_EK75_WIRED_PID            0x0045
#define DAREU_EK75_VENDOR_INTERFACE     3

/*---------------------------------------------------------*\
| Protocol                                                  |
|                                                           |
| Every exchange is one 64 byte HID feature report, no      |
| report ID:                                                |
|                                                           |
|   [0] target   0 = the receiver, (slot + 1) << 4 = the    |
|                keyboard paired in that slot. In a reply   |
|                the low nibble is a status, 2 = ready      |
|   [1] size     payload bytes that follow the header       |
|   [2] class    DAREU_CLASS_*                              |
|   [3] command  DAREU_*_CMD_*, |0x80 (DAREU_CMD_GET) reads |
|   [4] profile  1 for effect and brightness, 0 otherwise   |
|   [5] 0                                                   |
|   [6..] payload                                           |
|                                                           |
| The reply has the same layout, so the payload of a reply  |
| starts at DAREU_REPLY_PAYLOAD (6) as well.                |
\*---------------------------------------------------------*/
#define DAREU_REPORT_SIZE               64
#define DAREU_CLASS_DEVICE              0
#define DAREU_CLASS_LIGHTING            3
#define DAREU_CMD_GET                   0x80
#define DAREU_DEV_CMD_WIRELESS_STATUS   32
#define DAREU_LED_CMD_ATTRIBUTE         1
#define DAREU_LED_CMD_EFFECT            2
#define DAREU_LED_CMD_BRIGHTNESS        3
#define DAREU_LED_CMD_FRAME             4       /* per-key frames, never sent: it wedges the receiver */

#define DAREU_REGION_KEYS               1
#define DAREU_REGION_SIDE_LIGHT         4

#define DAREU_EFFECT_OFF                0
#define DAREU_EFFECT_STATIC             1
#define DAREU_EFFECT_STREAMING_FRAME    18

#define DAREU_MAX_COLORS                5
#define DAREU_MIN_SPEED                 1
#define DAREU_MAX_SPEED                 3

/*---------------------------------------------------------*\
| Region description as reported by LED_CMD_ATTRIBUTE       |
\*---------------------------------------------------------*/
struct DareuRegionInfo
{
    unsigned char               region;
    unsigned char               type;
    unsigned char               fps;
    unsigned char               rows;
    unsigned char               columns;
    std::vector<unsigned char>  effects;
};

/*---------------------------------------------------------*\
| Effect state of one region                                |
|                                                           |
| flag is the direction, stored by the firmware but never   |
| rendered. An empty colour list is valid: the animated     |
| effects then cycle through the rainbow.                   |
\*---------------------------------------------------------*/
struct DareuEffectState
{
    unsigned char               effect      = DAREU_EFFECT_STATIC;
    unsigned char               flag        = 0;
    unsigned char               speed       = DAREU_MIN_SPEED;
    std::vector<RGBColor>       colors;
};

/*---------------------------------------------------------*\
| One HID channel shared by every region of the keyboard    |
|                                                           |
| The receiver handles one command at a time, so all        |
| transfers go through Transfer(), which serialises them    |
| with a mutex and spaces them out. The controllers of the  |
| regions share one instance through a shared_ptr.          |
\*---------------------------------------------------------*/
class DareuEK75Device
{
public:
    /*-----------------------------------------------------*\
    | device_pid is the PID that was detected (defaults to  |
    | the receiver's, so every existing call site that only |
    | ever talked to a receiver keeps compiling unchanged). |
    | It decides which path Connect() takes.                |
    \*-----------------------------------------------------*/
    DareuEK75Device(hid_device* dev_handle, const std::string& path, unsigned short device_pid = DAREU_EK75_RECEIVER_PID);
    ~DareuEK75Device();

    /*-----------------------------------------------------*\
    | Locate the keyboard: through the receiver's pairing   |
    | handshake, or directly at target 0 when wired         |
    \*-----------------------------------------------------*/
    bool                        Connect();

    std::string                 GetLocation();
    unsigned short              GetKeyboardPID();

    bool                        GetRegionInfo(unsigned char region, DareuRegionInfo& info);
    bool                        GetEffect(unsigned char region, DareuEffectState& state);
    bool                        SetEffect(unsigned char region, const DareuEffectState& state);
    bool                        GetBrightness(unsigned char region, unsigned char& brightness);
    bool                        SetBrightness(unsigned char region, unsigned char brightness);

private:
    bool                        Transfer(unsigned char target, unsigned char size, unsigned char cls, unsigned char command, unsigned char profile, const std::vector<unsigned char>& payload, unsigned char* reply);
    bool                        Lighting(unsigned char command, unsigned char size, unsigned char profile, const std::vector<unsigned char>& payload, unsigned char* reply);

    hid_device*                 dev;
    std::string                 location;
    std::mutex                  lock;
    unsigned char               target_id;
    unsigned short              pid;
    unsigned short              keyboard_pid;
    std::chrono::steady_clock::time_point last_transfer;
};

/*---------------------------------------------------------*\
| Facade for one lighting region                            |
\*---------------------------------------------------------*/
class DareuEK75Controller
{
public:
    DareuEK75Controller(std::shared_ptr<DareuEK75Device> device_ptr, unsigned char region_id, const DareuRegionInfo& region_info, std::string dev_name);

    std::string                 GetNameString();
    std::string                 GetDeviceLocation();
    unsigned char               GetRegion();
    const DareuRegionInfo&      GetRegionInfo();

    bool                        GetEffect(DareuEffectState& state);
    bool                        SetEffect(const DareuEffectState& state);
    bool                        GetBrightness(unsigned char& brightness);
    bool                        SetBrightness(unsigned char brightness);

private:
    std::shared_ptr<DareuEK75Device> device;
    unsigned char               region;
    DareuRegionInfo             info;
    std::string                 name;
};
