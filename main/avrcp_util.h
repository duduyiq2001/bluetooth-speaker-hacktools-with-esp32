#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_avrc_api.h"
#include "esp_a2dp_api.h"

static const char *AVR_TAG = "AVRCP_CAPABILITIES";
static uint8_t current_transaction_label = 0;

// AVRCP callback
void avrcp_ct_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);

// genrate avrcp initialization function
void avrcp_init();
