# TASK-C2.6a: tool contracts (name/description/schema) come from
# tools/catalogue/*.json, not C literals - see ethervox_tool_catalogue_load().
include(${CMAKE_SOURCE_DIR}/cmake/EmbedCatalogue.cmake)
ethervox_embed_catalogue(
    ${CMAKE_SOURCE_DIR}/tools/catalogue/compute_tools.json
    ETHERVOX_CATALOGUE_COMPUTE_TOOLS_JSON
    ${CMAKE_CURRENT_BINARY_DIR}/compute_tools_catalogue.h
)

add_library(ethervox_compute_tools STATIC
    calculator_plugin.c
    percentage_plugin.c
    time_query_plugin.c
    compute_tools_registry.c
)

target_include_directories(ethervox_compute_tools PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Link with math library, cJSON for the catalogue loader
target_link_libraries(ethervox_compute_tools m cJSON ethervox_governor)
