#set(COMPONENT_ADD_INCLUDEDIRS include)
#set(COMPONENT_SRCS "src/wifi_station.cpp")
#set(COMPONENT_NAME "Smartknob-HA SDK Wifi_STATION")

idf_component_register( SRCS "src/wifi_station.cpp"
                        INCLUDE_DIRS "include"
                        REQUIRES esp_wifi esp_event esp_netif lwip)

#component_compile_options(-std=c++17)