add_library(ethervox_plugins STATIC
    plugin_manager.c
)

target_include_directories(ethervox_plugins PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Platform-specific linking for dynamic library loading
if(TARGET_PLATFORM STREQUAL "WINDOWS")
    # Windows doesn't need additional libraries for LoadLibrary/FreeLibrary
    
elseif(TARGET_PLATFORM STREQUAL "LINUX" OR TARGET_PLATFORM STREQUAL "RPI")
    # Link with libdl for dlopen/dlclose
    target_link_libraries(ethervox_plugins dl)
    
elseif(TARGET_PLATFORM STREQUAL "ESP32")
    # ESP32 doesn't support dynamic loading, use static plugins only
    target_compile_definitions(ethervox_plugins PRIVATE ETHERVOX_STATIC_PLUGINS=1)
endif()