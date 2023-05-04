set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS "src/messages.cpp")

set(COMPONENT_NAME "Smartknob-HA SDK Messages")

idf_component_register(COMPONENT_SRCS ${COMPONENT_SRS}
                    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
                    REQUIRES etl)