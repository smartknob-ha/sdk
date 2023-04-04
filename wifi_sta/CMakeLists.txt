idf_component_register( SRCS "src/wifi_sta.cpp"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES esp_wifi esp_event nvs_flash lwip vfs)