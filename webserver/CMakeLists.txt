idf_component_register( SRCS "src/Webserver.cpp"
                        INCLUDE_DIRS "include"
                        REQUIRES manager esp_http_server
                        PRIV_REQUIRES util )