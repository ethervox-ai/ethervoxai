add_library(ethervox_dialogue STATIC
    dialogue_core.c
)

target_include_directories(ethervox_dialogue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Link with math library
target_link_libraries(ethervox_dialogue m)