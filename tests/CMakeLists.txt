# CMakeLists.txt for EthervoxAI tests
cmake_minimum_required(VERSION 3.16)

# Include the main project's source files for testing
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Enable testing
enable_testing()

# Find all unit test source files
file(GLOB UNIT_TEST_SOURCES "unit/test_*.c")

# Create test executables for each unit test
foreach(TEST_SOURCE ${UNIT_TEST_SOURCES})
    # Get the test name from the filename
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    # Create executable
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    # Link against the main project sources (excluding main.cpp)
    file(GLOB_RECURSE ETHERVOXAI_TEST_SOURCES 
        "${CMAKE_SOURCE_DIR}/src/audio/*.c"
        "${CMAKE_SOURCE_DIR}/src/dialogue/*.c" 
        "${CMAKE_SOURCE_DIR}/src/platform/*.c"
        "${CMAKE_SOURCE_DIR}/src/plugins/*.c"
    )
    target_sources(${TEST_NAME} PRIVATE ${ETHERVOXAI_TEST_SOURCES})
    
    # Platform-specific linking (same as main CMakeLists.txt)
    if(WIN32)
        target_link_libraries(${TEST_NAME} winmm ws2_32 crypt32)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(${TEST_NAME} asound pthread curl ssl crypto)
    elseif(APPLE)
        target_link_libraries(${TEST_NAME} "-framework CoreAudio" "-framework AudioUnit" "-framework CoreFoundation" pthread curl)
    endif()
    
    # Add the test to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    # Set test properties
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 30
        FAIL_REGULAR_EXPRESSION "FAILED"
    )
endforeach()

# Integration tests (if any)
file(GLOB INTEGRATION_TEST_SOURCES "integration/test_*.c")

foreach(TEST_SOURCE ${INTEGRATION_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    set(TEST_NAME "${TEST_NAME}_integration")
    
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_sources(${TEST_NAME} PRIVATE ${ETHERVOXAI_TEST_SOURCES})
    
    if(WIN32)
        target_link_libraries(${TEST_NAME} winmm ws2_32 crypt32)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(${TEST_NAME} asound pthread curl ssl crypto)
    elseif(APPLE)
        target_link_libraries(${TEST_NAME} "-framework CoreAudio" "-framework AudioUnit" "-framework CoreFoundation" pthread curl)
    endif()
    
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "FAILED"
    )
endforeach()

# Test summary target
add_custom_target(test-summary
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS ${UNIT_TEST_TARGETS} ${INTEGRATION_TEST_TARGETS}
    COMMENT "Running all EthervoxAI tests"
)

# Coverage target (if gcov is available)
find_program(GCOV_PATH gcov)
if(GCOV_PATH AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} --coverage")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} --coverage")
    
    add_custom_target(coverage
        COMMAND lcov --capture --directory . --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND lcov --list coverage.info
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating code coverage report"
    )
endif()