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
    set(EXAMPLE_CORE_SOURCES)
    
    # Include common sources with absolute paths (error and logging)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/common/error.c")
        list(APPEND EXAMPLE_CORE_SOURCES "${CMAKE_SOURCE_DIR}/src/common/error.c")
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/common/logging.c")
        list(APPEND EXAMPLE_CORE_SOURCES "${CMAKE_SOURCE_DIR}/src/common/logging.c")
    endif()
    
    if(DEFINED ETHERVOXAI_CORE_SOURCES)
        list(APPEND EXAMPLE_CORE_SOURCES ${ETHERVOXAI_CORE_SOURCES})
    endif()
    if(DEFINED PLATFORM_SOURCES)
        foreach(_platform_src ${PLATFORM_SOURCES})
            if(IS_ABSOLUTE ${_platform_src})
                list(APPEND EXAMPLE_CORE_SOURCES ${_platform_src})
            else()
                list(APPEND EXAMPLE_CORE_SOURCES "${CMAKE_SOURCE_DIR}/${_platform_src}")
            endif()
        endforeach()
    endif()

    # Core runtime pieces not captured by top-level glob
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/audio/audio_core.c")
        list(APPEND EXAMPLE_CORE_SOURCES "${CMAKE_SOURCE_DIR}/src/audio/audio_core.c")
    endif()

    file(GLOB _example_audio_processing "${CMAKE_SOURCE_DIR}/src/audio_processing/*.c")
    if(_example_audio_processing)
        list(APPEND EXAMPLE_CORE_SOURCES ${_example_audio_processing})
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/src/platform/platform_core.c")
        list(APPEND EXAMPLE_CORE_SOURCES "${CMAKE_SOURCE_DIR}/src/platform/platform_core.c")
    endif()

    list(REMOVE_ITEM EXAMPLE_CORE_SOURCES "${CMAKE_SOURCE_DIR}/src/main.c")
    list(REMOVE_ITEM EXAMPLE_CORE_SOURCES "src/main.c")
    list(REMOVE_DUPLICATES EXAMPLE_CORE_SOURCES)

    add_executable(voice_assistant_demo
        ${VOICE_ASSISTANT_SOURCES}
        ${EXAMPLE_CORE_SOURCES}
    )

    target_include_directories(voice_assistant_demo PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )

    get_property(_ethervox_platform_libs GLOBAL PROPERTY ETHERVOXAI_PLATFORM_LIBS)
    if(_ethervox_platform_libs)
        target_link_libraries(voice_assistant_demo PRIVATE ${_ethervox_platform_libs})
    endif()

    # Mirror compile definitions from core target when available
    if(TARGET ethervoxai)
        get_target_property(_core_defs ethervoxai COMPILE_DEFINITIONS)
        if(_core_defs)
            target_compile_definitions(voice_assistant_demo PRIVATE ${_core_defs})
        endif()
    endif()
endif()
