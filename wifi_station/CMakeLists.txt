set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_SRCS "src/wifi_station.cpp")
set(COMPONENT_NAME "Smartknob-HA SDK Wifi_STA")

idf_component_register()
#component_compile_options(-std=c++17)