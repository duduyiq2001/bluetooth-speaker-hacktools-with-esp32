idf_component_register(SRCS "mac_addr_spoof.c" "bt_hci_common.c" "http_server.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash esp_http_server esp_wifi spiffs)
