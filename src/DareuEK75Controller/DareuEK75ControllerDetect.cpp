/*---------------------------------------------------------*\
| DareuEK75ControllerDetect.cpp                             |
|                                                           |
|   Detector for the Dareu EK75 keyboard                    |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <memory>
#include <hidapi.h>
#include "DetectionManager.h"
#include "DareuEK75Controller.h"
#include "RGBController_DareuEK75.h"
#include "LogManager.h"

/*---------------------------------------------------------*\
| The keyboard is reached through the 2.4G receiver. Its    |
| configuration channel is the vendor feature report on     |
| interface 3.                                              |
\*---------------------------------------------------------*/
DetectedControllers DetectDareuEK75(hid_device_info* info, const std::string& name)
{
    DetectedControllers detected_controllers;
    hid_device*         dev = hid_open_path(info->path);

    if(!dev)
    {
        return(detected_controllers);
    }

    /*-----------------------------------------------------*\
    | The device closes the handle when the last region     |
    | controller releases it                                |
    \*-----------------------------------------------------*/
    std::shared_ptr<DareuEK75Device> device = std::make_shared<DareuEK75Device>(dev, info->path);

    if(!device->Connect())
    {
        LOG_INFO("[Dareu EK75] No keyboard connected to the receiver, is it switched to 2.4G?");
        return(detected_controllers);
    }

    struct
    {
        unsigned char   region;
        const char*     suffix;
    } regions[] =
    {
        { DAREU_REGION_KEYS,        ""              },
        { DAREU_REGION_SIDE_LIGHT,  " Side Light"   },
    };

    for(const auto& region : regions)
    {
        DareuRegionInfo region_info;

        if(!device->GetRegionInfo(region.region, region_info))
        {
            continue;
        }

        DareuEK75Controller*        controller      = new DareuEK75Controller(device, region.region, region_info, name + region.suffix);
        RGBController_DareuEK75*    rgb_controller  = new RGBController_DareuEK75(controller);

        detected_controllers.push_back(rgb_controller);
    }

    return(detected_controllers);
}

REGISTER_HID_DETECTOR_I("Dareu EK75", DetectDareuEK75, DAREU_VID, DAREU_EK75_RECEIVER_PID, DAREU_EK75_RECEIVER_INTERFACE);
