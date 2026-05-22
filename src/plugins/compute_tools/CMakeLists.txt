add_library(ethervox_compute_tools STATIC
    calculator_plugin.c
    percentage_plugin.c
    time_query_plugin.c
    compute_tools_registry.c
)

target_include_directories(ethervox_compute_tools PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Link with math library
target_link_libraries(ethervox_compute_tools m)
