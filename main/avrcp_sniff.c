#include "avrcp_sniff.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

// Sniffer task: reads from fd and logs every frame
void l2cap_sniffer_task(void *arg) {
    int fd = (intptr_t)arg;
    uint8_t buf[512];
    while (true) {
        int len = read(fd, buf, sizeof(buf));           // POSIX read :contentReference[oaicite:4]{index=4}
        if (len > 0) {
            ESP_LOG_BUFFER_HEX(L2_TAG, buf, len);          // Hex dump :contentReference[oaicite:5]{index=5}
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));              // Yield if no data
        }
    }
    vTaskDelete(NULL);
}

// L2CAP callback: handles OPEN and CLOSE events
void bt_l2cap_cb(esp_bt_l2cap_cb_event_t event,
                        esp_bt_l2cap_cb_param_t *param) {
    switch (event) {
    case ESP_BT_L2CAP_INIT_EVT:
        ESP_LOGI(L2_TAG, "L2CAP initialized");              // Initialization complete :contentReference[oaicite:6]{index=6}
        break;

    case ESP_BT_L2CAP_OPEN_EVT: {
        int fd = param->open.fd;                        // fd provided here :contentReference[oaicite:7]{index=7}
        ESP_LOGI(L2_TAG, "Channel open, fd=%d peer address=0x%0gx", fd, param->open.rem_bda);
        // Launch sniffer task to continuously read this fd
        xTaskCreate(l2cap_sniffer_task, "sniffer", 4096,
                    (void*)(intptr_t)fd, 5, NULL);        // FreeRTOS task create :contentReference[oaicite:8]{index=8}
        break;
    }

    case ESP_BT_L2CAP_CLOSE_EVT:
        ESP_LOGI(L2_TAG, "Channel closed");
        // Optionally perform cleanup or task deletion here :contentReference[oaicite:9]{index=9}
        break;

    default:
        break;
    }
}

void initialize_l2cap(){
    // 2. Initialize L2CAP module and register our callback :contentReference[oaicite:1]{index=1}
    esp_err_t ret = esp_bt_l2cap_init();
    ESP_ERROR_CHECK(ret);
    esp_err_t ret1 = esp_bt_l2cap_register_callback(bt_l2cap_cb);
    ESP_ERROR_CHECK(ret1); 
}

void avrcp_sniff(esp_bd_addr_t target_bda){
    esp_err_t ret = esp_bt_l2cap_connect(avrcp_psm, avrcp_psm, target_bda);
    if (ret != ESP_OK) {
        ESP_LOGE(L2_TAG, "L2CAP connect failed: %d", ret);
        return;
    }
    ESP_LOGI(L2_TAG, "L2CAP connect succeeded: %d", ret);
}