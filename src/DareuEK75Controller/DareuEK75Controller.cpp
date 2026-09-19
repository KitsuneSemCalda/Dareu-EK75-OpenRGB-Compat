/*---------------------------------------------------------*\
| DareuEK75Controller.cpp                                   |
|                                                           |
|   Driver for the Dareu EK75 keyboard (TK51G) through its  |
|   2.4G receiver                                           |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <string.h>
#include <thread>
#include "DareuEK75Controller.h"
#include "LogManager.h"

using namespace std::chrono_literals;

/*---------------------------------------------------------*\
| The receiver answers asynchronously: after a feature      |
| report is sent, GET_FEATURE is polled until the low       |
| nibble of the first reply byte is 2. Commands sent too    |
| close together are dropped, so keep a minimum gap.        |
\*---------------------------------------------------------*/
#define DAREU_REPLY_READY_MASK          0x0F
#define DAREU_REPLY_READY_VALUE         0x02
#define DAREU_REPLY_POLL_COUNT          20
#define DAREU_REPLY_POLL_DELAY          10ms
#define DAREU_MIN_TRANSFER_GAP          20ms

/*---------------------------------------------------------*\
| Offsets inside the 64 byte reply (report ID stripped)     |
\*---------------------------------------------------------*/
#define DAREU_REPLY_PAYLOAD             6

DareuEK75Device::DareuEK75Device(hid_device* dev_handle, const std::string& path)
{
    dev             = dev_handle;
    location        = path;
    target_id       = 0;
    keyboard_pid    = 0;
    last_transfer   = std::chrono::steady_clock::now();
}

DareuEK75Device::~DareuEK75Device()
{
    hid_close(dev);
}

std::string DareuEK75Device::GetLocation()
{
    return("HID: " + location);
}

unsigned short DareuEK75Device::GetKeyboardPID()
{
    return(keyboard_pid);
}

bool DareuEK75Device::Transfer(unsigned char target, unsigned char size, unsigned char cls, unsigned char command, unsigned char profile, const std::vector<unsigned char>& payload, unsigned char* reply)
{
    /*-----------------------------------------------------*\
    | LED_CMD_FRAME wedges the receiver until it is         |
    | unplugged, and the firmware has no per-key support    |
    | anyway. Never send it.                                |
    \*-----------------------------------------------------*/
    if(cls == DAREU_CLASS_LIGHTING && (command & ~DAREU_CMD_GET) == DAREU_LED_CMD_FRAME)
    {
        LOG_ERROR("[Dareu EK75] Refusing to send LED_CMD_FRAME, it hangs the receiver");
        return(false);
    }

    if(payload.size() > (DAREU_REPORT_SIZE - DAREU_REPLY_PAYLOAD))
    {
        return(false);
    }

    std::lock_guard<std::mutex> guard(lock);

    unsigned char buf[DAREU_REPORT_SIZE + 1];
    memset(buf, 0x00, sizeof(buf));

    /*-----------------------------------------------------*\
    | buf[0] is the report ID, which the device does not    |
    | declare and is always 0                               |
    \*-----------------------------------------------------*/
    buf[1] = target;
    buf[2] = size;
    buf[3] = cls;
    buf[4] = command;
    buf[5] = profile;
    memcpy(&buf[1 + DAREU_REPLY_PAYLOAD], payload.data(), payload.size());

    std::this_thread::sleep_until(last_transfer + DAREU_MIN_TRANSFER_GAP);

    int result = hid_send_feature_report(dev, buf, sizeof(buf));

    if(result < 0)
    {
        LOG_DEBUG("[Dareu EK75] hid_send_feature_report failed");
        last_transfer = std::chrono::steady_clock::now();
        return(false);
    }

    bool ready = false;

    for(unsigned int i = 0; i < DAREU_REPLY_POLL_COUNT && !ready; i++)
    {
        std::this_thread::sleep_for(DAREU_REPLY_POLL_DELAY);

        memset(buf, 0x00, sizeof(buf));
        result = hid_get_feature_report(dev, buf, sizeof(buf));

        if(result > 0 && (buf[1] & DAREU_REPLY_READY_MASK) == DAREU_REPLY_READY_VALUE)
        {
            ready = true;
        }
    }

    last_transfer = std::chrono::steady_clock::now();

    if(!ready || buf[3] != cls)
    {
        LOG_DEBUG("[Dareu EK75] No valid reply to class %d command 0x%02X", cls, command);
        return(false);
    }

    memcpy(reply, &buf[1], DAREU_REPORT_SIZE);

    return(true);
}

bool DareuEK75Device::Lighting(unsigned char command, unsigned char size, unsigned char profile, const std::vector<unsigned char>& payload, unsigned char* reply)
{
    return(Transfer(target_id, size, DAREU_CLASS_LIGHTING, command, profile, payload, reply));
}

bool DareuEK75Device::Connect()
{
    /*-----------------------------------------------------*\
    | Ask the receiver itself (target 0) which keyboards    |
    | are paired. Slot n is addressed as (n + 1) << 4.      |
    \*-----------------------------------------------------*/
    unsigned char reply[DAREU_REPORT_SIZE];

    if(!Transfer(0, 7, DAREU_CLASS_DEVICE, DAREU_DEV_CMD_WIRELESS_STATUS | DAREU_CMD_GET, 0, {}, reply))
    {
        return(false);
    }

    unsigned int slots = reply[DAREU_REPLY_PAYLOAD];

    for(unsigned int slot = 0; slot < slots && (DAREU_REPLY_PAYLOAD + 1 + (3 * slot) + 2) < DAREU_REPORT_SIZE; slot++)
    {
        const unsigned char* entry = &reply[DAREU_REPLY_PAYLOAD + 1 + (3 * slot)];

        if(entry[0] != 0)
        {
            target_id       = (unsigned char)((slot + 1) << 4);
            keyboard_pid    = (unsigned short)((entry[1] << 8) | entry[2]);

            LOG_DEBUG("[Dareu EK75] Keyboard PID 0x%04X on slot %d, target 0x%02X", keyboard_pid, slot, target_id);

            return(true);
        }
    }

    LOG_DEBUG("[Dareu EK75] Receiver has no connected keyboard");

    return(false);
}

bool DareuEK75Device::GetRegionInfo(unsigned char region, DareuRegionInfo& info)
{
    unsigned char reply[DAREU_REPORT_SIZE];

    if(!Lighting(DAREU_LED_CMD_ATTRIBUTE | DAREU_CMD_GET, 1, 0, { region }, reply) || reply[DAREU_REPLY_PAYLOAD] != region)
    {
        return(false);
    }

    unsigned int count = reply[DAREU_REPLY_PAYLOAD + 5];

    if((DAREU_REPLY_PAYLOAD + 6 + count) > DAREU_REPORT_SIZE)
    {
        return(false);
    }

    info.region     = region;
    info.type       = reply[DAREU_REPLY_PAYLOAD + 1];
    info.fps        = reply[DAREU_REPLY_PAYLOAD + 2];
    info.rows       = reply[DAREU_REPLY_PAYLOAD + 3];
    info.columns    = reply[DAREU_REPLY_PAYLOAD + 4];
    info.effects.assign(&reply[DAREU_REPLY_PAYLOAD + 6], &reply[DAREU_REPLY_PAYLOAD + 6 + count]);

    return(true);
}

bool DareuEK75Device::GetEffect(unsigned char region, DareuEffectState& state)
{
    unsigned char reply[DAREU_REPORT_SIZE];

    if(!Lighting(DAREU_LED_CMD_EFFECT | DAREU_CMD_GET, 1, 1, { region }, reply) || reply[DAREU_REPLY_PAYLOAD] != region)
    {
        return(false);
    }

    unsigned int count = reply[DAREU_REPLY_PAYLOAD + 4];

    if(count > DAREU_MAX_COLORS)
    {
        return(false);
    }

    state.effect    = reply[DAREU_REPLY_PAYLOAD + 1];
    state.flag      = reply[DAREU_REPLY_PAYLOAD + 2];
    state.speed     = reply[DAREU_REPLY_PAYLOAD + 3];
    state.colors.clear();

    for(unsigned int i = 0; i < count; i++)
    {
        const unsigned char* rgb = &reply[DAREU_REPLY_PAYLOAD + 5 + (3 * i)];

        state.colors.push_back(ToRGBColor(rgb[0], rgb[1], rgb[2]));
    }

    return(true);
}

bool DareuEK75Device::SetEffect(unsigned char region, const DareuEffectState& state)
{
    unsigned char reply[DAREU_REPORT_SIZE];

    unsigned int count = (unsigned int)state.colors.size();

    if(count > DAREU_MAX_COLORS)
    {
        count = DAREU_MAX_COLORS;
    }

    std::vector<unsigned char> payload = { region, state.effect, state.flag, state.speed, (unsigned char)count };

    for(unsigned int i = 0; i < count; i++)
    {
        payload.push_back(RGBGetRValue(state.colors[i]));
        payload.push_back(RGBGetGValue(state.colors[i]));
        payload.push_back(RGBGetBValue(state.colors[i]));
    }

    if(!Lighting(DAREU_LED_CMD_EFFECT, (unsigned char)(5 + (3 * count)), 1, payload, reply))
    {
        return(false);
    }

    /*-----------------------------------------------------*\
    | The reply echoes the effect that was applied          |
    \*-----------------------------------------------------*/
    return(reply[DAREU_REPLY_PAYLOAD] == region && reply[DAREU_REPLY_PAYLOAD + 1] == state.effect);
}

bool DareuEK75Device::GetBrightness(unsigned char region, unsigned char& brightness)
{
    unsigned char reply[DAREU_REPORT_SIZE];

    if(!Lighting(DAREU_LED_CMD_BRIGHTNESS | DAREU_CMD_GET, 2, 1, { region }, reply) || reply[DAREU_REPLY_PAYLOAD] != region)
    {
        return(false);
    }

    brightness = reply[DAREU_REPLY_PAYLOAD + 1];

    return(true);
}

bool DareuEK75Device::SetBrightness(unsigned char region, unsigned char brightness)
{
    unsigned char reply[DAREU_REPORT_SIZE];

    return(Lighting(DAREU_LED_CMD_BRIGHTNESS, 2, 1, { region, brightness }, reply));
}

/*---------------------------------------------------------*\
| DareuEK75Controller                                       |
\*---------------------------------------------------------*/
DareuEK75Controller::DareuEK75Controller(std::shared_ptr<DareuEK75Device> device_ptr, unsigned char region_id, const DareuRegionInfo& region_info, std::string dev_name)
{
    device  = device_ptr;
    region  = region_id;
    info    = region_info;
    name    = dev_name;
}

std::string DareuEK75Controller::GetNameString()
{
    return(name);
}

std::string DareuEK75Controller::GetDeviceLocation()
{
    return(device->GetLocation() + " region " + std::to_string(region));
}

unsigned char DareuEK75Controller::GetRegion()
{
    return(region);
}

const DareuRegionInfo& DareuEK75Controller::GetRegionInfo()
{
    return(info);
}

bool DareuEK75Controller::GetEffect(DareuEffectState& state)
{
    return(device->GetEffect(region, state));
}

bool DareuEK75Controller::SetEffect(const DareuEffectState& state)
{
    return(device->SetEffect(region, state));
}

bool DareuEK75Controller::GetBrightness(unsigned char& brightness)
{
    return(device->GetBrightness(region, brightness));
}

bool DareuEK75Controller::SetBrightness(unsigned char brightness)
{
    return(device->SetBrightness(region, brightness));
}
