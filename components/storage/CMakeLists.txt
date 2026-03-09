idf_component_register(
    SRCS
        "memory.c"
        "memory_manager.c"
    INCLUDE_DIRS 
        "." 
    REQUIRES
        utils
        littlefs
)

set(COMPONENT_KCONFIG "Kconfig")
