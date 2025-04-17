// summmary
// This code defines a set of constants and functions to handle Bluetooth Class of Device (CoD) information.
// It includes enums for major and minor device classes, service classes, and functions to print CoD details and check if a device is an audio device.
//
#ifndef COD_HELPER
#define COD_HELPER

#include <stdio.h>
#include <stdint.h>


static const char* COD_TAG = "BT_COD";

// Major Service Classes (bits 16-23)
typedef enum {
    BT_SERVICE_POSITIONING    = (1 << 3),
    BT_SERVICE_NETWORKING     = (1 << 4),
    BT_SERVICE_RENDERING      = (1 << 5),
    BT_SERVICE_CAPTURING      = (1 << 6),
    BT_SERVICE_OBJECT_TRANSFER= (1 << 7),
    BT_SERVICE_AUDIO          = (1 << 8),
    BT_SERVICE_TELEPHONY      = (1 << 9),
    BT_SERVICE_INFORMATION    = (1 << 10)
} bt_service_class_t;

// Major Device Classes (bits 8-12)
typedef enum {
    BT_MAJOR_MISC            = 0x00,
    BT_MAJOR_COMPUTER        = 0x01,
    BT_MAJOR_PHONE           = 0x02,
    BT_MAJOR_LAN             = 0x03,
    BT_MAJOR_AUDIO_VIDEO     = 0x04,
    BT_MAJOR_PERIPHERAL      = 0x05,
    BT_MAJOR_IMAGING         = 0x06,
    BT_MAJOR_WEARABLE        = 0x07,
    BT_MAJOR_TOY             = 0x08,
    BT_MAJOR_HEALTH          = 0x09,
    BT_MAJOR_UNCATEGORIZED   = 0x1F
} bt_major_class_t;

// Audio/Video Minor Device Classes (bits 0-7 when Major=0x04)
typedef enum {
    BT_MINOR_AV_UNCATEGORIZED = 0x00,
    BT_MINOR_AV_WEARABLE_HEADSET = 0x01,
    BT_MINOR_AV_HANDSFREE    = 0x02,
    BT_MINOR_AV_MICROPHONE   = 0x04,
    BT_MINOR_AV_LOUDSPEAKER  = 0x05,
    BT_MINOR_AV_HEADPHONES   = 0x06,
    BT_MINOR_AV_PORTABLE_AUDIO = 0x07,
    BT_MINOR_AV_CAR_AUDIO    = 0x08,
    BT_MINOR_AV_SETTOP_BOX   = 0x09,
    BT_MINOR_AV_HIFI_AUDIO   = 0x0A,
    BT_MINOR_AV_VCR          = 0x0B,
    BT_MINOR_AV_VIDEO_CAMERA = 0x0C,
    BT_MINOR_AV_CAMCORDER    = 0x0D,
    BT_MINOR_AV_VIDEO_MONITOR = 0x0E,
    BT_MINOR_AV_VIDEO_DISPLAY = 0x0F,
    BT_MINOR_AV_VIDEO_CONFERENCING = 0x10,
    BT_MINOR_AV_GAMING_DEVICE = 0x12
} bt_minor_av_class_t;
#endif
void print_cod_details(uint32_t cod);
bool is_audio_device(uint32_t cod);