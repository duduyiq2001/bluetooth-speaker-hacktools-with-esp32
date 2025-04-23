idf_component_register(SRCS "dos.c" "cod_helper.c" "avrcp_sniff.c" "bt_hci_common.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash)
