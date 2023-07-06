idf_component_register( SRCS "src/wifi_ap.cpp"
                        INCLUDE_DIRS "include"
                        REQUIRES esp_wifi manager
                        PRIV_REQUIRES esp_event nvs_flash lwip vfs)