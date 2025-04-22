#include "avrcp_util.h"
// AVRCP callback

// Global flag to indicate if AVRCP is ready
bool avrcp_ready = false;

void avrcp_ct_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    switch (event)
    {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
        ESP_LOGI(AVR_TAG, "AVRCP Connection state: %s", param->conn_stat.connected ? "Connected" : "Disconnected");
        if (param->conn_stat.connected)
        {
            // Fetch supported features once connected
            esp_avrc_ct_send_get_rn_capabilities_cmd(current_transaction_label);
        }
        break;

    case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
        ESP_LOGI(AVR_TAG, "Speaker capabilities (supported events):");
        // Pointer to the GetCapabilities response parameters
    struct avrc_ct_get_rn_caps_rsp_param *p = &(param->get_rn_caps_rsp);

    // Test operation
    const esp_avrc_bit_mask_op_t op = ESP_AVRC_BIT_MASK_OP_TEST;
        // Check PLAY_STATUS_CHANGE
        if (esp_avrc_rn_evt_bit_mask_operation(op, &(p->evt_set), ESP_AVRC_RN_PLAY_STATUS_CHANGE))
        {
            ESP_LOGI(AVR_TAG, "Peer supports PLAY_STATUS_CHANGE");
        }

        // Check TRACK_CHANGE
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_TRACK_CHANGE))
        {
            ESP_LOGI(AVR_TAG, "Peer supports TRACK_CHANGE");
        }

        // Check TRACK_REACHED_END
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_TRACK_REACHED_END))
        {
            ESP_LOGI(AVR_TAG, "Peer supports TRACK_REACHED_END");
        }

        // Check TRACK_REACHED_START
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_TRACK_REACHED_START))
        {
            ESP_LOGI(AVR_TAG, "Peer supports TRACK_REACHED_START");
        }

        // Check PLAY_POS_CHANGED
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_PLAY_POS_CHANGED))
        {
            ESP_LOGI(AVR_TAG, "Peer supports PLAY_POS_CHANGED");
        }

        // Check BATTERY_STATUS_CHANGE
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_BATTERY_STATUS_CHANGE))
        {
            ESP_LOGI(AVR_TAG, "Peer supports BATTERY_STATUS_CHANGE");
        }

        // Check SYSTEM_STATUS_CHANGE
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_SYSTEM_STATUS_CHANGE))
        {
            ESP_LOGI(AVR_TAG, "Peer supports SYSTEM_STATUS_CHANGE");
        }

        // Check APP_SETTING_CHANGE
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_APP_SETTING_CHANGE))
        {
            ESP_LOGI(AVR_TAG, "Peer supports APP_SETTING_CHANGE");
        }

        // Check NOW_PLAYING_CHANGE
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_NOW_PLAYING_CHANGE))
        {
            ESP_LOGI(AVR_TAG, "Peer supports NOW_PLAYING_CHANGE");
        }

        // Check AVAILABLE_PLAYERS_CHANGE
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_AVAILABLE_PLAYERS_CHANGE))
        {
            ESP_LOGI(AVR_TAG, "Peer supports AVAILABLE_PLAYERS_CHANGE");
        }

        // Check ADDRESSED_PLAYER_CHANGE
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_ADDRESSED_PLAYER_CHANGE))
        {
            ESP_LOGI(AVR_TAG, "Peer supports ADDRESSED_PLAYER_CHANGE");
        }

        // Check UIDS_CHANGE
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_UIDS_CHANGE))
        {
            ESP_LOGI(AVR_TAG, "Peer supports UIDS_CHANGE");
        }

        // Check VOLUME_CHANGE
        if (esp_avrc_rn_evt_bit_mask_operation(op, &p->evt_set, ESP_AVRC_RN_VOLUME_CHANGE))
        {
            ESP_LOGI(AVR_TAG, "Peer supports VOLUME_CHANGE");
        }
        break;

    default:
        ESP_LOGI(AVR_TAG, "Unhandled AVRCP event: %d", event);
        break;
    }
}

// avrcp initialization function
void avrcp_init()
{
    ESP_LOGI(AVR_TAG, "Initializing AVRCP controller");
    esp_err_t ret = esp_avrc_ct_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(AVR_TAG, "Failed to initialize AVRCP: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(AVR_TAG, "Registering AVRCP callback");
    ret = esp_avrc_ct_register_callback(avrcp_ct_callback);
    if (ret != ESP_OK)
    {
        ESP_LOGE(AVR_TAG, "Failed to register AVRCP callback: %s", esp_err_to_name(ret));
        return;
    }
    
    // Mark AVRCP as ready immediately since there's no initialization event
    avrcp_ready = true;
    ESP_LOGI(AVR_TAG, "AVRCP successfully initialized");
}
