#ifndef AVRCP_CNIFF_H
#define AVRCP_CNIFF_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_l2cap_bt_api.h"
#include "esp_log.h"
#include <string.h>

static const char *L2_TAG = "L2CAP_SNIFFER";
static const uint16_t avrcp_psm = 0x0017;

void l2cap_sniffer_task(void *arg);
void bt_l2cap_cb(esp_bt_l2cap_cb_event_t event,
                 esp_bt_l2cap_cb_param_t *param);
void initialize_l2cap();
void avrcp_sniff(esp_bd_addr_t target_bda);
#endif // AVRCP_CNIFF_H