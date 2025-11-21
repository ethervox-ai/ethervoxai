# CMake configuration for EtherVoxAI examples

cmake_minimum_required(VERSION 3.16)

# Voice Assistant demo -----------------------------------------------------
set(VOICE_ASSISTANT_SOURCES)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/voice_assistant/main.c")
    list(APPEND VOICE_ASSISTANT_SOURCES voice_assistant/main.c)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/voice_assistant/pipeline.c")
    list(APPEND VOICE_ASSISTANT_SOURCES voice_assistant/pipeline.c)
endif()

if(VOICE_ASSISTANT_SOURCES)
    add_executable(voice_assistant_demo ${VOICE_ASSISTANT_SOURCES})

    target_include_directories(voice_assistant_demo PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )

    # Link against the ethervoxai static library which contains all platform sources
    target_link_libraries(voice_assistant_demo PRIVATE ethervoxai)

    # Get platform-specific system libraries
    get_property(_ethervox_platform_libs GLOBAL PROPERTY ETHERVOXAI_PLATFORM_LIBS)
    if(_ethervox_platform_libs)
        target_link_libraries(voice_assistant_demo PRIVATE ${_ethervox_platform_libs})
    endif()

    # Mirror compile definitions from core target
    if(TARGET ethervoxai)
        get_target_property(_core_defs ethervoxai COMPILE_DEFINITIONS)
        if(_core_defs)
            target_compile_definitions(voice_assistant_demo PRIVATE ${_core_defs})
        endif()
    endif()
endif()
