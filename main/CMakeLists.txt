idf_component_register(SRCS "dos_attack.c" "get_speaker_capabilities.c" "cod_helper.c" "avrcp_sniff.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash)
