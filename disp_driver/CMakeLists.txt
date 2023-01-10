# set(COMPONENT_ADD_INCLUDEDIRS include)
# set(COMPONENT_SRCS "src/display.cpp")
# set(COMPONENT_NAME "Smartknob-HA SDK Display")

idf_component_register( SRCS "src/display.cpp"
                        INCLUDE_DIRS "include"
                        REQUIRES lvgl lvgl_esp32_drivers)