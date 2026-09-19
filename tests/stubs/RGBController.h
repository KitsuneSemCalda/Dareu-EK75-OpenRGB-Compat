#pragma once
#include <string>
#include <vector>

/*---------------------------------------------------------*\
| Mirrors the parts of OpenRGB's RGBController API that the |
| driver uses, with OpenRGB's names and values              |
\*---------------------------------------------------------*/
typedef unsigned int RGBColor;
#define RGBGetRValue(rgb)   (rgb & 0x000000FF)
#define RGBGetGValue(rgb)   ((rgb >> 8) & 0x000000FF)
#define RGBGetBValue(rgb)   ((rgb >> 16) & 0x000000FF)
#define ToRGBColor(r, g, b) ((RGBColor)((b << 16) | (g << 8) | (r)))

enum
{
    MODE_FLAG_HAS_SPEED                 = (1 << 0),
    MODE_FLAG_HAS_BRIGHTNESS            = (1 << 4),
    MODE_FLAG_HAS_PER_LED_COLOR         = (1 << 5),
    MODE_FLAG_HAS_MODE_SPECIFIC_COLOR   = (1 << 6),
    MODE_FLAG_HAS_RANDOM_COLOR          = (1 << 7),
    MODE_FLAG_AUTOMATIC_SAVE            = (1 << 9),
};

enum
{
    MODE_COLORS_NONE            = 0,
    MODE_COLORS_PER_LED         = 1,
    MODE_COLORS_MODE_SPECIFIC   = 2,
    MODE_COLORS_RANDOM          = 3,
};

enum
{
    ZONE_TYPE_SINGLE,
    ZONE_TYPE_LINEAR,
    ZONE_TYPE_MATRIX,
};

enum
{
    DEVICE_TYPE_LEDSTRIP,
    DEVICE_TYPE_KEYBOARD,
};

class mode
{
public:
    std::string             name;
    int                     value           = 0;
    unsigned int            flags           = 0;
    unsigned int            speed_min       = 0;
    unsigned int            speed_max       = 0;
    unsigned int            brightness_min  = 0;
    unsigned int            brightness_max  = 0;
    unsigned int            colors_min      = 0;
    unsigned int            colors_max      = 0;
    unsigned int            speed           = 0;
    unsigned int            brightness      = 0;
    unsigned int            color_mode      = MODE_COLORS_NONE;
    std::vector<RGBColor>   colors;
};

class led
{
public:
    std::string             name;
};

struct matrix_map_type
{
    unsigned int            height  = 0;
    unsigned int            width   = 0;
};

class zone
{
public:
    std::string             name;
    int                     type        = ZONE_TYPE_SINGLE;
    unsigned int            leds_min    = 0;
    unsigned int            leds_max    = 0;
    unsigned int            leds_count  = 0;
    matrix_map_type         matrix_map;
};

class RGBController
{
public:
    virtual ~RGBController() {}

    std::string             name;
    std::string             vendor;
    std::string             description;
    std::string             location;
    int                     type        = 0;
    unsigned int            active_mode = 0;

    std::vector<mode>       modes;
    std::vector<zone>       zones;
    std::vector<led>        leds;
    std::vector<RGBColor>   colors;

    virtual void            SetupZones() = 0;
    virtual void            DeviceUpdateLEDs() = 0;
    virtual void            DeviceUpdateZoneLEDs(int zone) = 0;
    virtual void            DeviceUpdateSingleLED(int led) = 0;
    virtual void            DeviceUpdateMode() = 0;

    void SetupColors()
    {
        colors.resize(leds.size());
    }

    void Shutdown() {}

    /*-----------------------------------------------------*\
    | What the OpenRGB core does when a client asks for a   |
    | change: update the state, then call the Device* hook  |
    \*-----------------------------------------------------*/
    void UpdateLEDs()               { DeviceUpdateLEDs(); }
    void UpdateMode()               { DeviceUpdateMode(); }
    void SetActiveMode(unsigned int m)  { active_mode = m; DeviceUpdateMode(); }
};
