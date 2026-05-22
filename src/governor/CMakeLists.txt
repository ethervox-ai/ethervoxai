# Feature flag: Use streaming governor implementation
option(ETHERVOX_USE_STREAMING_GOVERNOR "Use new streaming governor architecture" OFF)  # Temporarily OFF for testing

# Core helper functions (always included)
add_library(ethervox_governor STATIC
    governor_helpers.c
    tool_registry.c
    chat_template.c
)

# Conditionally include governor implementation
if(ETHERVOX_USE_STREAMING_GOVERNOR)
    message(STATUS "Using streaming governor implementation")
    target_sources(ethervox_governor PRIVATE governor_streaming.c)
    target_compile_definitions(ethervox_governor PUBLIC ETHERVOX_USE_STREAMING_GOVERNOR=1)
else()
    message(STATUS "Using legacy governor implementation")
    target_sources(ethervox_governor PRIVATE governor.c)
endif()

target_include_directories(ethervox_governor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Link with math library
target_link_libraries(ethervox_governor m)

# ============================================================================
# Unit Tests (optional)
# ============================================================================

option(ETHERVOX_BUILD_TESTS "Build unit tests" OFF)

if(ETHERVOX_BUILD_TESTS)
    message(STATUS "Building governor unit tests")
    
    # Test for helper functions
    add_executable(test_governor_helpers
        ${CMAKE_SOURCE_DIR}/tests/test_governor_helpers.c
    )
    target_link_libraries(test_governor_helpers ethervox_governor)
    target_include_directories(test_governor_helpers PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
    )
    
    # Test for streaming implementation
    if(ETHERVOX_USE_STREAMING_GOVERNOR)
        add_executable(test_governor_streaming
            ${CMAKE_SOURCE_DIR}/tests/test_governor_streaming.c
        )
        target_link_libraries(test_governor_streaming ethervox_governor)
        target_include_directories(test_governor_streaming PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/include
        )
    endif()
    
    # Add tests to CTest
    enable_testing()
    add_test(NAME HelperFunctions COMMAND test_governor_helpers)
    if(ETHERVOX_USE_STREAMING_GOVERNOR)
        add_test(NAME StreamingGovernor COMMAND test_governor_streaming)
    endif()
endif()
