idf_component_register( SRCS "src/AccessPoint.cpp" "src/Station.cpp"
                        INCLUDE_DIRS "include"
                        REQUIRES esp_wifi manager
                        PRIV_REQUIRES esp_event nvs_flash lwip vfs util)