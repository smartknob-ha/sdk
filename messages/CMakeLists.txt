set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS "src/messages.cpp")

idf_component_register(COMPONENT_SRCS ${COMPONENT_SRS}
                    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS})