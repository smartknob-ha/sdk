set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_SRCS "src/wifi_ap.cpp")

set(COMPONENT_NAME "Smartknob-HA SDK Wifi_AP")

idf_component_register()
#component_compile_options(-std=c++17)