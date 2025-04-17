idf_component_register(SRCS "speaker_exploit.c" "cod_helper.c" "avrcp_sniff.c"
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash)
