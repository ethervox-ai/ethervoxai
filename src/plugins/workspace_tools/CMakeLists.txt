# Workspace Tools Plugin CMake Configuration

# Create workspace_tools library
add_library(workspace_tools STATIC
    workspace_tools.c
)

# Include directories
target_include_directories(workspace_tools PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Link with ethervox core (for logging and registry)
target_link_libraries(workspace_tools
    ethervox_core
)

# Platform-specific settings
if(WIN32)
    # Windows-specific compiler flags if needed
    target_compile_definitions(workspace_tools PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Installation
install(TARGETS workspace_tools
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES workspace_operations.h
    DESTINATION include/ethervox/plugins
)
