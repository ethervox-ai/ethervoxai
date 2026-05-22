# Conversation Tools Plugin
# Provides LLM-controlled conversational interaction tools

add_library(conversation_tools OBJECT
    speak.c
    listen.c
    conversation_tools_registry.c
)

target_include_directories(conversation_tools PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(conversation_tools PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
)
