set(include_dirs include ./include)
list(APPEND srcs "src/tedge_command.c" "src/tedge.c")

idf_component_register(SRCS "${srcs}"
    INCLUDE_DIRS "${include_dirs}")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
