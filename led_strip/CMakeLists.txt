set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS "src/led_strip.cpp")

set(COMPONENT_NAME "Smartknob-HA SDK/led strip")

idf_component_register(SRCS ${COMPONENT_SRCS}
                    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS})