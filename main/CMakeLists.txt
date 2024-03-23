idf_component_register(SRCS "main.c" "components/tedge_ota.c" "../tedge/src/tedge_command.c" "../tedge/src/tedge.c"
                    INCLUDE_DIRS "." "components/include" "../tedge/include")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
