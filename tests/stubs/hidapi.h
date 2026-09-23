#pragma once
#include <stddef.h>

struct hid_device_;
typedef struct hid_device_ hid_device;

struct hid_device_info
{
    char*           path;
    unsigned short  product_id = 0;
};

extern "C"
{
    hid_device* hid_open_path(const char* path);
    void        hid_close(hid_device* dev);
    int         hid_send_feature_report(hid_device* dev, const unsigned char* data, size_t length);
    int         hid_get_feature_report(hid_device* dev, unsigned char* data, size_t length);
}
