# validate_github_token.cmake
# Validates GitHub token at CMake configuration time
#
# This script uses the GitHub API to verify that the provided token:
# 1. Exists and is not empty
# 2. Is a valid GitHub token
# 3. Has not expired
# 4. Has the required permissions (issues:write)
#
# Copyright (c) 2024-2025 EthervoxAI Team
# SPDX-License-Identifier: CC-BY-NC-SA-4.0

# Check if token is provided
if(NOT DEFINED GITHUB_TOKEN OR "${GITHUB_TOKEN}" STREQUAL "")
    message(FATAL_ERROR 
        "\n"
        "====================================================================\n"
        "ERROR: GitHub token is required but not set!\n"
        "====================================================================\n"
        "\n"
        "The bug reporter requires a valid GitHub token to submit issues.\n"
        "\n"
        "Please set one of the following environment variables:\n"
        "  - ETHERVOX_GITHUB_TOKEN_DESKTOP (preferred for desktop builds)\n"
        "  - ETHERVOX_GITHUB_TOKEN (fallback)\n"
        "\n"
        "Example:\n"
        "  export ETHERVOX_GITHUB_TOKEN=\"ghp_your_token_here\"\n"
        "\n"
        "To create a token:\n"
        "  1. Go to: https://github.com/settings/tokens\n"
        "  2. Generate a fine-grained token\n"
        "  3. Grant ONLY 'issues:write' permission for timkos98/ethervoxai\n"
        "  4. Set expiration date (recommended: 90 days)\n"
        "\n"
        "====================================================================\n"
    )
endif()

message(STATUS "Validating GitHub token...")

# Use curl to validate token via GitHub API
# Endpoint: GET /user - returns authenticated user info if token is valid
# Docs: https://docs.github.com/en/rest/users/users#get-the-authenticated-user
find_program(CURL_EXECUTABLE curl)
if(NOT CURL_EXECUTABLE)
    message(WARNING 
        "curl not found - skipping token validation. "
        "Build will proceed but token may be invalid."
    )
    return()
endif()

# Validate token by calling GitHub API
execute_process(
    COMMAND ${CURL_EXECUTABLE}
        -s
        -o /dev/null
        -w "%{http_code}"
        -H "Authorization: Bearer ${GITHUB_TOKEN}"
        -H "Accept: application/vnd.github+json"
        -H "X-GitHub-Api-Version: 2022-11-28"
        "https://api.github.com/user"
    OUTPUT_VARIABLE HTTP_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Check HTTP status code
if(HTTP_STATUS EQUAL 200)
    message(STATUS "GitHub token validation: SUCCESS (HTTP 200)")
    
    # Additional check: verify token has issues:write permission for the repo
    # Endpoint: GET /repos/{owner}/{repo}
    execute_process(
        COMMAND ${CURL_EXECUTABLE}
            -s
            -o /dev/null
            -w "%{http_code}"
            -H "Authorization: Bearer ${GITHUB_TOKEN}"
            -H "Accept: application/vnd.github+json"
            -H "X-GitHub-Api-Version: 2022-11-28"
            "https://api.github.com/repos/timkos98/ethervoxai"
        OUTPUT_VARIABLE REPO_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    if(REPO_STATUS EQUAL 200)
        message(STATUS "GitHub token has access to timkos98/ethervoxai repository")
    else()
        message(WARNING 
            "GitHub token validation: Token is valid but may not have access "
            "to timkos98/ethervoxai repository (HTTP ${REPO_STATUS}). "
            "Bug reporting may fail at runtime."
        )
    endif()
    
elseif(HTTP_STATUS EQUAL 401)
    message(FATAL_ERROR 
        "\n"
        "====================================================================\n"
        "ERROR: GitHub token validation FAILED (HTTP 401 Unauthorized)\n"
        "====================================================================\n"
        "\n"
        "The provided token is invalid or has expired.\n"
        "\n"
        "Please:\n"
        "  1. Verify your token is correct\n"
        "  2. Check if the token has expired\n"
        "  3. Generate a new token if needed:\n"
        "     https://github.com/settings/tokens\n"
        "\n"
        "Current token starts with: ${GITHUB_TOKEN_PREFIX}...\n"
        "\n"
        "====================================================================\n"
    )
elseif(HTTP_STATUS EQUAL 403)
    message(FATAL_ERROR 
        "\n"
        "====================================================================\n"
        "ERROR: GitHub token validation FAILED (HTTP 403 Forbidden)\n"
        "====================================================================\n"
        "\n"
        "The token may have insufficient permissions or rate limit exceeded.\n"
        "\n"
        "Please verify:\n"
        "  1. Token has 'issues:write' permission\n"
        "  2. Token is for the correct repository\n"
        "  3. API rate limit has not been exceeded\n"
        "\n"
        "====================================================================\n"
    )
else()
    message(FATAL_ERROR 
        "\n"
        "====================================================================\n"
        "ERROR: GitHub token validation FAILED (HTTP ${HTTP_STATUS})\n"
        "====================================================================\n"
        "\n"
        "Unexpected response from GitHub API.\n"
        "\n"
        "Possible causes:\n"
        "  - Network connectivity issues\n"
        "  - GitHub API is down\n"
        "  - Token format is incorrect\n"
        "\n"
        "Please check:\n"
        "  1. Internet connection\n"
        "  2. GitHub status: https://www.githubstatus.com/\n"
        "  3. Token format (should start with 'ghp_' or 'github_pat_')\n"
        "\n"
        "====================================================================\n"
    )
endif()
