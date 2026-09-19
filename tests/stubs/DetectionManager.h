#pragma once
#include <functional>
#include <string>
#include <vector>
#include <hidapi.h>
#include "RGBController.h"

typedef std::vector<RGBController*> DetectedControllers;

/*---------------------------------------------------------*\
| Detectors register themselves at static-init time, as in  |
| OpenRGB. Tests read this list to check what was registered|
\*---------------------------------------------------------*/
struct TestHIDDetector
{
    std::string     name;
    DetectedControllers (*func)(hid_device_info*, const std::string&);
    unsigned short  vid;
    unsigned short  pid;
    int             interface_number;
};

std::vector<TestHIDDetector>& TestHIDDetectors();

struct TestHIDDetectorRegistrar
{
    TestHIDDetectorRegistrar(const char* name, DetectedControllers (*func)(hid_device_info*, const std::string&), unsigned short vid, unsigned short pid, int interface_number)
    {
        TestHIDDetectors().push_back({ name, func, vid, pid, interface_number });
    }
};

#define REGISTER_HID_DETECTOR_I(name, func, vid, pid, interface) \
    static TestHIDDetectorRegistrar device_detector_obj_##vid##pid##_##interface(name, func, vid, pid, interface)
