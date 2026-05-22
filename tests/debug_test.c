// Quick debug test
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

bool contains_tool_json_debug(const char* lookahead_buffer) {
    if (!lookahead_buffer || lookahead_buffer[0] == '\0') {
        return false;
    }
    
    size_t buf_len = strlen(lookahead_buffer);
    
    // Check for JSON opening brace at start
    if (buf_len > 0 && (lookahead_buffer[0] == '{' || 
        (buf_len >= 2 && lookahead_buffer[0] == '{' && lookahead_buffer[1] == '"'))) {
        printf("  DEBUG: Matched opening brace at start\n");
        return true;
    }
    
    // Check for tool JSON indicators
    const char* tool_json_indicators[] = {
        "{\"name\":",
        "\"arguments\":", "\"name\":", "\"location\":", "\"forecast_type\":",
        "\"expression\":", "\"duration_",
        "},",
        "}\n",
        "\"},",
        "\"}\n",
        NULL
    };
    
    for (int i = 0; tool_json_indicators[i] != NULL; i++) {
        if (strstr(lookahead_buffer, tool_json_indicators[i]) != NULL) {
            printf("  DEBUG: Matched pattern '%s'\n", tool_json_indicators[i]);
            return true;
        }
    }
    
    return false;
}

bool should_discard_buffer_debug(const char* combined_lookahead) {
    if (!combined_lookahead || combined_lookahead[0] == '\0') {
        return false;
    }
    
    size_t buf_len = strlen(combined_lookahead);
    
    // Check for JSON opening at start
    if (buf_len > 0 && (combined_lookahead[0] == '{' || 
        (buf_len >= 2 && combined_lookahead[0] == '{' && combined_lookahead[1] == '"'))) {
        printf("  DEBUG: Flush matched opening brace\n");
        return true;
    }
    
    const char* tool_json_patterns[] = {
        "{\"name\":",
        "\"arguments\":",
        "\"name\":",
        "\"location\":",
        "\"forecast_type\":",
        "\"expression\":",
        "\"duration_",
        "},",
        "}\n",
        "\"},",
        "\"}\n",
        "</tool_call>",
        NULL
    };
    
    for (int i = 0; tool_json_patterns[i] != NULL; i++) {
        if (strstr(combined_lookahead, tool_json_patterns[i]) != NULL) {
            printf("  DEBUG: Flush matched pattern '%s'\n", tool_json_patterns[i]);
            return true;
        }
    }
    
    return false;
}

int main() {
    printf("Test 1: allows_brace_mid_sentence\n");
    const char* test1 = "The value is { x: 10 }";
    printf("Buffer: '%s'\n", test1);
    bool result1 = contains_tool_json_debug(test1);
    printf("Result: %s (expected: false)\n\n", result1 ? "true" : "false");
    
    printf("Test 2: flush_discards_partial_tool_json\n");
    const char* test2 = "arguments\": {\"location\"";
    printf("Buffer: '%s'\n", test2);
    bool result2 = should_discard_buffer_debug(test2);
    printf("Result: %s (expected: true)\n\n", result2 ? "true" : "false");
    
    return 0;
}
