set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS "src/Manager.cpp")

set(COMPONENT_NAME "Smartknob-HA SDK Manager")

idf_component_register(SRCS ${COMPONENT_SRCS}
                    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
                    REQUIRES config_provider
                    PRIV_REQUIRES util)