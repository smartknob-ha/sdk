set(COMPONENT_NAME "Smartknob-HA SDK Mock component")

idf_component_register( SRCS "src/MockComponent.cpp"
                        INCLUDE_DIRS "include"
                        REQUIRES manager)