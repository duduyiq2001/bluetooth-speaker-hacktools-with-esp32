idf_component_register(SRCS "get_speaker_capabilities.c" "cod_helper.c" "avrcp_sniff.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash)
