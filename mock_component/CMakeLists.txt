set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS "src/mock_component.cpp")

set(COMPONENT_NAME "Smartknob-HA SDK Mock component")

idf_component_register(SRCS ${COMPONENT_SRCS}
                    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS})