idf_component_register(
    SRCS
        "app_main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        openharmony_liteos
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    OHOS_CALL_LOS_KERNELINIT=1
    OHOS_CALL_LOS_START=1
)
