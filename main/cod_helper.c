
#include <esp_log.h>
#include "cod_helper.h"

/// Function to print Class of Device (CoD) details
/// @param cod The Class of Device value to be printed
/// This function extracts and prints the service classes, major device class,
/// minor device class, and their respective values from the CoD.
/// It also checks if the device is an audio device based on the CoD value.
/// The function uses ESP_LOGI to log the information.
/// The function is useful for debugging and understanding the capabilities
/// of Bluetooth devices based on their CoD.
void print_cod_details(uint32_t cod) {
    // Extract components
    uint32_t service_classes = (cod >> 16) & 0xFF;
    uint8_t major_class = (cod >> 8) & 0x1F;
    uint8_t minor_class = cod & 0xFF;
    
    // Print raw COD
    ESP_LOGI(COD_TAG, "CoD: 0x%06X", cod);
    
    // Print Service Classes
    ESP_LOGI(COD_TAG, "Service Classes:");
    if (service_classes == 0) {
        ESP_LOGI(COD_TAG, "  - None");
    } else {
        if (service_classes & BT_SERVICE_POSITIONING) ESP_LOGI(COD_TAG, "  - Positioning");
        if (service_classes & BT_SERVICE_NETWORKING) ESP_LOGI(COD_TAG, "  - Networking");
        if (service_classes & BT_SERVICE_RENDERING) ESP_LOGI(COD_TAG, "  - Rendering");
        if (service_classes & BT_SERVICE_CAPTURING) ESP_LOGI(COD_TAG, "  - Capturing");
        if (service_classes & BT_SERVICE_OBJECT_TRANSFER) ESP_LOGI(COD_TAG, "  - Object Transfer");
        if (service_classes & BT_SERVICE_AUDIO) ESP_LOGI(COD_TAG, "  - Audio");
        if (service_classes & BT_SERVICE_TELEPHONY) ESP_LOGI(COD_TAG, "  - Telephony");
        if (service_classes & BT_SERVICE_INFORMATION) ESP_LOGI(COD_TAG, "  - Information");
    }
    
    // Print Major Device Class
    ESP_LOGI(COD_TAG, "Major Device Class:");
    switch (major_class) {
        case BT_MAJOR_MISC:          ESP_LOGI(COD_TAG, "  - Miscellaneous"); break;
        case BT_MAJOR_COMPUTER:      ESP_LOGI(COD_TAG, "  - Computer"); break;
        case BT_MAJOR_PHONE:         ESP_LOGI(COD_TAG, "  - Phone"); break;
        case BT_MAJOR_LAN:           ESP_LOGI(COD_TAG, "  - LAN/Network Access"); break;
        case BT_MAJOR_AUDIO_VIDEO:   ESP_LOGI(COD_TAG, "  - Audio/Video"); break;
        case BT_MAJOR_PERIPHERAL:    ESP_LOGI(COD_TAG, "  - Peripheral"); break;
        case BT_MAJOR_IMAGING:       ESP_LOGI(COD_TAG, "  - Imaging"); break;
        case BT_MAJOR_WEARABLE:      ESP_LOGI(COD_TAG, "  - Wearable"); break;
        case BT_MAJOR_TOY:           ESP_LOGI(COD_TAG, "  - Toy"); break;
        case BT_MAJOR_HEALTH:        ESP_LOGI(COD_TAG, "  - Health"); break;
        case BT_MAJOR_UNCATEGORIZED: ESP_LOGI(COD_TAG, "  - Uncategorized"); break;
        default:                     ESP_LOGI(COD_TAG, "  - Unknown (0x%02X)", major_class); break;
    }
    
    // Print Minor Device Class (only for Audio/Video)
    if (major_class == BT_MAJOR_AUDIO_VIDEO) {
        ESP_LOGI(COD_TAG, "Minor Device Class (Audio/Video):");
        switch (minor_class) {
            case BT_MINOR_AV_UNCATEGORIZED:      ESP_LOGI(COD_TAG, "  - Uncategorized"); break;
            case BT_MINOR_AV_WEARABLE_HEADSET:   ESP_LOGI(COD_TAG, "  - Wearable Headset"); break;
            case BT_MINOR_AV_HANDSFREE:          ESP_LOGI(COD_TAG, "  - Hands-free"); break;
            case BT_MINOR_AV_MICROPHONE:         ESP_LOGI(COD_TAG, "  - Microphone"); break;
            case BT_MINOR_AV_LOUDSPEAKER:        ESP_LOGI(COD_TAG, "  - Loudspeaker"); break;
            case BT_MINOR_AV_HEADPHONES:         ESP_LOGI(COD_TAG, "  - Headphones"); break;
            case BT_MINOR_AV_PORTABLE_AUDIO:     ESP_LOGI(COD_TAG, "  - Portable Audio"); break;
            case BT_MINOR_AV_CAR_AUDIO:          ESP_LOGI(COD_TAG, "  - Car Audio"); break;
            case BT_MINOR_AV_HIFI_AUDIO:         ESP_LOGI(COD_TAG, "  - HiFi Audio"); break;
            default:                             ESP_LOGI(COD_TAG, "  - Unknown (0x%02X)", minor_class); break;
        }
    } else {
        ESP_LOGI(COD_TAG, "Minor Device Class: 0x%02X (Not Audio/Video)", minor_class);
    }
}
/// Function to check if a device is an audio device based on its CoD
/// @param cod The Class of Device value to be checked
/// @return true if the device is an audio device, false otherwise
/// This function checks the Major Device Class and relevant service classes
/// to determine if the device is an audio device.
/// It is useful for filtering devices during Bluetooth discovery.
/// The function uses bitwise operations to extract the necessary information
/// from the CoD value.
bool is_audio_device(uint32_t cod) {
    // Check Major Device Class is Audio/Video (0x04)
    uint8_t major_class = (cod >> 8) & 0x1F;
    if (major_class != BT_MAJOR_AUDIO_VIDEO) {
        return false;
    }
    
    // Check for relevant service classes
    uint32_t service_classes = (cod >> 16) & 0xFF;
    if (!(service_classes & (BT_SERVICE_AUDIO | BT_SERVICE_RENDERING))) {
        return false;
    }
    return true;
    
}