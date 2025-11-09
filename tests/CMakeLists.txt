# SPDX-License-Identifier: CC-BY-NC-SA-4.0

# Error handling tests
add_executable(test_error unit/test_error.c)
target_link_libraries(test_error ethervoxai)
target_include_directories(test_error PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME ErrorHandling COMMAND test_error)
set_tests_properties(ErrorHandling PROPERTIES TIMEOUT 30 LABELS "unit")
