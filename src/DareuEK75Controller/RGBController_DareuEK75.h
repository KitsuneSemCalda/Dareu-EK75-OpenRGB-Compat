/*---------------------------------------------------------*\
| RGBController_DareuEK75.h                                 |
|                                                           |
|   RGBController for one lighting region of the Dareu EK75 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBController.h"
#include "DareuEK75Controller.h"

class RGBController_DareuEK75 : public RGBController
{
public:
    RGBController_DareuEK75(DareuEK75Controller* controller_ptr);
    ~RGBController_DareuEK75();

    void SetupZones();

    void DeviceUpdateLEDs();
    void DeviceUpdateZoneLEDs(int zone);
    void DeviceUpdateSingleLED(int led);

    void DeviceUpdateMode();

private:
    void                    SetupModes();
    void                    LoadCurrentState();
    void                    ApplyMode();

    DareuEK75Controller*    controller;
    int                     last_brightness;
    RGBColor                last_direct_color;
};
