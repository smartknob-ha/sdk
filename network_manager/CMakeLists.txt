idf_component_register( SRCS "src/NetworkManager.cpp"
                        INCLUDE_DIRS "include"
                        REQUIRES wifi manager
                        PRIV_REQUIRES util esp_netif esp_wifi lwip )