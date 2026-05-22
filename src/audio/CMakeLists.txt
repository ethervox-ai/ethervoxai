add_library(ethervox_audio STATIC
    audio_core.c
    platform_macos.c
    platform_windows.c
    platform_linux.c
    platform_esp32.c
    platform_rpi.c
    reference_buffer.c
    aec_speex.c
)

target_include_directories(ethervox_audio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Find Speex for AEC
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(SPEEX speexdsp)
    if(SPEEX_FOUND)
        target_link_libraries(ethervox_audio ${SPEEX_LIBRARIES})
        target_include_directories(ethervox_audio PRIVATE ${SPEEX_INCLUDE_DIRS})
        message(STATUS "Speex DSP found: ${SPEEX_VERSION}")
    else()
        message(WARNING "Speex DSP not found - AEC will use passthrough mode. Install with: brew install speex")
    endif()
else()
    message(WARNING "PkgConfig not found - skipping Speex detection")
endif()

# Platform-specific linking
if(TARGET_PLATFORM STREQUAL "WINDOWS")
    target_link_libraries(ethervox_audio winmm)
    
elseif(TARGET_PLATFORM STREQUAL "LINUX")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED alsa)
    target_link_libraries(ethervox_audio ${ALSA_LIBRARIES})
    target_include_directories(ethervox_audio PRIVATE ${ALSA_INCLUDE_DIRS})
    
elseif(TARGET_PLATFORM STREQUAL "RPI")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED alsa)
    target_link_libraries(ethervox_audio ${ALSA_LIBRARIES} bcm2835)
    target_include_directories(ethervox_audio PRIVATE ${ALSA_INCLUDE_DIRS})
    
elseif(TARGET_PLATFORM STREQUAL "ESP32")
    # ESP32 libraries will be linked by ESP-IDF build system
    target_compile_definitions(ethervox_audio PRIVATE ESP_PLATFORM=1)
endif()

# Add math library for sine wave generation
target_link_libraries(ethervox_audio m)