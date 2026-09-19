/*---------------------------------------------------------*\
| RGBController_DareuEK75.cpp                               |
|                                                           |
|   RGBController for one lighting region of the Dareu EK75 |
|                                                           |
|   The firmware has no per-key colour on this model (both  |
|   regions are LedType 4), so every region is a single    |
|   zone with one LED and the modes are the firmware's own  |
|   effects.                                                |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <algorithm>
#include "RGBController_DareuEK75.h"
#include "LogManager.h"

/*---------------------------------------------------------*\
| Mode values above 0xFF are software modes that map onto   |
| the Static effect                                         |
\*---------------------------------------------------------*/
#define DAREU_MODE_DIRECT               0x100
#define DAREU_MODE_KEYS_OFF             0x101

typedef struct
{
    unsigned char   effect;
    const char*     name;
    bool            has_speed;
    bool            has_color;
    unsigned int    max_colors;
} dareu_effect_info;

/*---------------------------------------------------------*\
| Effect names from the vendor's TG_LIGHT_EFFECT_INDEX.     |
| Speed and colour support follow the vendor's own tables   |
| (see docs/RESEARCH.md). Neon clears its colour list.      |
\*---------------------------------------------------------*/
static const dareu_effect_info dareu_effects[] =
{
    {  0, "Off",                false,  false,  0 },
    {  1, "Static",             false,  true,   1 },
    {  2, "Breathing",          true,   true,   DAREU_MAX_COLORS },
    {  3, "Neon",               true,   false,  0 },
    {  4, "Reactive",           true,   true,   1 },
    {  5, "Wave",               true,   true,   1 },
    {  6, "Raindrop",           false,  true,   1 },
    {  7, "Gather",             false,  true,   1 },
    {  8, "Ripple",             false,  true,   1 },
    {  9, "Running Light",      true,   true,   1 },
    { 10, "Rotate",             true,   true,   1 },
    { 11, "Starlit",            true,   true,   2 },
    { 12, "Heat Up",            false,  true,   1 },
    { 19, "Lap",                true,   true,   1 },
    { 20, "Rainbow Wave",       true,   true,   1 },
    { 21, "Light Wave",         true,   true,   1 },
    { 22, "Steady Stream",      true,   true,   1 },
    { 23, "Start Up",           false,  true,   1 },
    { 24, "Area Reactive",      true,   true,   1 },
    { 25, "Line Reactive",      true,   true,   1 },
    { 26, "Waterfall",          true,   true,   1 },
    { 27, "Scanning",           true,   true,   1 },
    { 28, "Heartbeat",          true,   true,   1 },
    { 29, "Fluxay",             true,   true,   1 },
    { 30, "Heart Breath",       false,  true,   1 },
    { 31, "Moon Breath",        false,  true,   1 },
    { 32, "Star Breath",        false,  true,   1 },
};

static const dareu_effect_info* FindEffect(unsigned char effect)
{
    for(const dareu_effect_info& info : dareu_effects)
    {
        if(info.effect == effect)
        {
            return(&info);
        }
    }

    return(nullptr);
}

RGBController_DareuEK75::RGBController_DareuEK75(DareuEK75Controller* controller_ptr)
{
    controller          = controller_ptr;
    last_brightness     = -1;
    last_direct_color   = 0;

    bool is_keys        = (controller->GetRegion() == DAREU_REGION_KEYS);

    name                = controller->GetNameString();
    vendor              = "Dareu";
    type                = is_keys ? DEVICE_TYPE_KEYBOARD : DEVICE_TYPE_LEDSTRIP;
    description         = name;
    location            = controller->GetDeviceLocation();

    SetupModes();
    SetupZones();
    LoadCurrentState();
}

RGBController_DareuEK75::~RGBController_DareuEK75()
{
    Shutdown();

    delete controller;
}

void RGBController_DareuEK75::SetupModes()
{
    const bool is_keys = (controller->GetRegion() == DAREU_REGION_KEYS);

    /*-----------------------------------------------------*\
    | Direct and Off are emulated with the Static effect,   |
    | the firmware has no Off effect on the key matrix      |
    \*-----------------------------------------------------*/
    if(is_keys)
    {
        mode Direct;
        Direct.name             = "Direct";
        Direct.value            = DAREU_MODE_DIRECT;
        Direct.flags            = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_BRIGHTNESS;
        Direct.color_mode       = MODE_COLORS_PER_LED;
        Direct.brightness_min   = 0;
        Direct.brightness_max   = 255;
        Direct.brightness       = 255;
        modes.push_back(Direct);
    }

    std::vector<unsigned char> supported = controller->GetRegionInfo().effects;
    std::sort(supported.begin(), supported.end());

    for(unsigned char effect : supported)
    {
        /*-------------------------------------------------*\
        | Streaming needs per-frame data the firmware never |
        | renders correctly, skip anything that is unknown  |
        \*-------------------------------------------------*/
        const dareu_effect_info* info = FindEffect(effect);

        if(info == nullptr || effect == DAREU_EFFECT_STREAMING_FRAME)
        {
            continue;
        }

        /*-------------------------------------------------*\
        | Region 4 ignores the colour of Static and shows a |
        | fixed pattern                                     |
        \*-------------------------------------------------*/
        const bool has_color = info->has_color && !(!is_keys && effect == DAREU_EFFECT_STATIC);

        mode m;
        m.name                  = info->name;
        m.value                 = effect;
        m.flags                 = MODE_FLAG_AUTOMATIC_SAVE;

        if(effect != DAREU_EFFECT_OFF)
        {
            m.flags            |= MODE_FLAG_HAS_BRIGHTNESS;
            m.brightness_min    = 0;
            m.brightness_max    = 255;
            m.brightness        = 255;
        }

        if(info->has_speed)
        {
            m.flags            |= MODE_FLAG_HAS_SPEED;
            m.speed_min         = DAREU_MIN_SPEED;
            m.speed_max         = DAREU_MAX_SPEED;
            m.speed             = DAREU_MIN_SPEED;
        }

        if(has_color)
        {
            m.flags            |= MODE_FLAG_HAS_MODE_SPECIFIC_COLOR;

            /*---------------------------------------------*\
            | An empty colour list makes the animated       |
            | effects cycle through the rainbow             |
            \*---------------------------------------------*/
            if(effect != DAREU_EFFECT_STATIC)
            {
                m.flags        |= MODE_FLAG_HAS_RANDOM_COLOR;
            }

            m.color_mode        = MODE_COLORS_MODE_SPECIFIC;
            m.colors_min        = 1;
            m.colors_max        = info->max_colors;
            m.colors.resize(1);
            m.colors[0]         = ToRGBColor(255, 255, 255);
        }
        else
        {
            m.color_mode        = MODE_COLORS_NONE;
        }

        modes.push_back(m);
    }

    if(is_keys)
    {
        mode Off;
        Off.name                = "Off";
        Off.value               = DAREU_MODE_KEYS_OFF;
        Off.flags               = MODE_FLAG_AUTOMATIC_SAVE;
        Off.color_mode          = MODE_COLORS_NONE;
        modes.push_back(Off);
    }
}

void RGBController_DareuEK75::SetupZones()
{
    zone new_zone;
    new_zone.name           = (controller->GetRegion() == DAREU_REGION_KEYS) ? "Keyboard" : "Side Light";
    new_zone.type           = ZONE_TYPE_SINGLE;
    new_zone.leds_min       = 1;
    new_zone.leds_max       = 1;
    new_zone.leds_count     = 1;
    new_zone.matrix_map.height  = 0;
    new_zone.matrix_map.width   = 0;
    zones.push_back(new_zone);

    led new_led;
    new_led.name            = new_zone.name;
    leds.push_back(new_led);

    SetupColors();
}

void RGBController_DareuEK75::LoadCurrentState()
{
    /*-----------------------------------------------------*\
    | Start from what the keyboard is showing so that       |
    | loading the device does not change the lighting       |
    \*-----------------------------------------------------*/
    DareuEffectState    state;
    unsigned char       brightness;

    if(controller->GetBrightness(brightness))
    {
        last_brightness = brightness;

        for(mode& m : modes)
        {
            if(m.flags & MODE_FLAG_HAS_BRIGHTNESS)
            {
                m.brightness = brightness;
            }
        }
    }

    if(!controller->GetEffect(state))
    {
        LOG_WARNING("[Dareu EK75] Could not read the current effect of region %d", controller->GetRegion());
        return;
    }

    for(unsigned int i = 0; i < modes.size(); i++)
    {
        if(modes[i].value != state.effect)
        {
            continue;
        }

        mode& m = modes[i];

        if((m.flags & MODE_FLAG_HAS_SPEED) && state.speed >= m.speed_min && state.speed <= m.speed_max)
        {
            m.speed = state.speed;
        }

        if(m.flags & MODE_FLAG_HAS_MODE_SPECIFIC_COLOR)
        {
            if(state.colors.empty())
            {
                if(m.flags & MODE_FLAG_HAS_RANDOM_COLOR)
                {
                    m.color_mode = MODE_COLORS_RANDOM;
                }
            }
            else
            {
                m.colors.assign(state.colors.begin(), state.colors.begin() + std::min<size_t>(state.colors.size(), m.colors_max));
            }
        }

        if(!state.colors.empty())
        {
            colors[0] = state.colors[0];
        }

        active_mode = i;

        return;
    }

    LOG_DEBUG("[Dareu EK75] Effect %d of region %d has no mode", state.effect, controller->GetRegion());
}

void RGBController_DareuEK75::ApplyMode()
{
    const mode& m = modes[active_mode];

    DareuEffectState state;

    if(m.value == DAREU_MODE_KEYS_OFF)
    {
        state.effect = DAREU_EFFECT_STATIC;
        state.colors.push_back(ToRGBColor(0, 0, 0));
    }
    else if(m.value == DAREU_MODE_DIRECT)
    {
        state.effect = DAREU_EFFECT_STATIC;
        state.colors.push_back(colors[0]);
        last_direct_color = colors[0];
    }
    else
    {
        state.effect = (unsigned char)m.value;

        if(m.flags & MODE_FLAG_HAS_SPEED)
        {
            state.speed = (unsigned char)m.speed;
        }

        if(m.color_mode == MODE_COLORS_MODE_SPECIFIC)
        {
            state.colors = m.colors;
        }
    }

    controller->SetEffect(state);

    if((m.flags & MODE_FLAG_HAS_BRIGHTNESS) && (int)m.brightness != last_brightness)
    {
        if(controller->SetBrightness((unsigned char)m.brightness))
        {
            last_brightness = (int)m.brightness;
        }
    }
}

void RGBController_DareuEK75::DeviceUpdateLEDs()
{
    /*-----------------------------------------------------*\
    | Only Direct follows the LED colour. The receiver      |
    | drops commands sent in bursts, so skip repeats.       |
    \*-----------------------------------------------------*/
    if(modes[active_mode].value == DAREU_MODE_DIRECT && colors[0] != last_direct_color)
    {
        ApplyMode();
    }
}

void RGBController_DareuEK75::DeviceUpdateZoneLEDs(int /*zone*/)
{
    DeviceUpdateLEDs();
}

void RGBController_DareuEK75::DeviceUpdateSingleLED(int /*led*/)
{
    DeviceUpdateLEDs();
}

void RGBController_DareuEK75::DeviceUpdateMode()
{
    ApplyMode();
}
