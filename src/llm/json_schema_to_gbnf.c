// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary

/**
 * @file json_schema_to_gbnf.c
 * @brief First-party JSON Schema → GBNF converter
 * 
 * Pure C implementation using only cJSON (no llama.cpp common/, no nlohmann/json).
 * Converts a closed subset of JSON Schema to GBNF for grammar-constrained decoding.
 * 
 * Implementation strategy:
 * - Type-by-type development with golden tests per type
 * - Recursive descent through schema tree
 * - String builder for GBNF output
 * - Rule counter for unique names in nested structures
 */

#include "ethervox/grammar.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// =============================================================================
// Grammar structure
// =============================================================================

struct ethervox_grammar {
    char* gbnf_source;    // Owned GBNF grammar source
    char* root_rule;      // Owned root rule name (usually "root")
    
    // Lazy grammar support
    bool is_lazy;         // Enable lazy mode
    char** trigger_words; // NULL-terminated array of trigger words
    size_t trigger_word_count;  // Count of trigger words
};

// =============================================================================
// String builder for GBNF construction
// =============================================================================

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} string_builder_t;

static ethervox_result_t sb_init(string_builder_t* sb, size_t initial_capacity) {
    sb->data = (char*)malloc(initial_capacity);
    if (!sb->data) return ETHERVOX_ERROR_OUT_OF_MEMORY;
    sb->data[0] = '\0';
    sb->length = 0;
    sb->capacity = initial_capacity;
    return ETHERVOX_SUCCESS;
}

static ethervox_result_t sb_append(string_builder_t* sb, const char* str) {
    size_t str_len = strlen(str);
    size_t required = sb->length + str_len + 1;
    
    if (required > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        while (new_capacity < required) new_capacity *= 2;
        
        char* new_data = (char*)realloc(sb->data, new_capacity);
        if (!new_data) return ETHERVOX_ERROR_OUT_OF_MEMORY;
        
        sb->data = new_data;
        sb->capacity = new_capacity;
    }
    
    memcpy(sb->data + sb->length, str, str_len + 1);
    sb->length += str_len;
    return ETHERVOX_SUCCESS;
}

static void sb_free(string_builder_t* sb) {
    free(sb->data);
    sb->data = NULL;
    sb->length = 0;
    sb->capacity = 0;
}

// =============================================================================
// Schema → GBNF conversion context
// =============================================================================

typedef struct {
    string_builder_t output;       // Final GBNF output
    string_builder_t helper_rules; // Helper rules (ws, etc.) to append at end
    int rule_counter;              // For unique rule names in nested structures
    int ws_emitted;                // Track if whitespace rule added
} conversion_context_t;

// Forward declarations
static ethervox_result_t emit_whitespace_rule(conversion_context_t* ctx);
static ethervox_result_t convert_schema_to_gbnf(
    conversion_context_t* ctx,
    const cJSON* schema,
    const char* rule_name
);
static ethervox_result_t convert_one_of(
    conversion_context_t* ctx,
    const cJSON* schema,
    const char* rule_name
);

// =============================================================================
// Type converters (implemented in order per C2.3a spec)
// =============================================================================

/**
 * Type 1: Boolean
 * {"type": "boolean"} → rule ::= "true" | "false"
 */
static ethervox_result_t convert_boolean(
    conversion_context_t* ctx,
    const char* rule_name
) {
    char rule[256];
    snprintf(rule, sizeof(rule), "%s ::= \"true\" | \"false\"\n", rule_name);
    return sb_append(&ctx->output, rule);
}

/**
 * Type 2: Integer
 * {"type": "integer"} → rule ::= "-"? [0-9]+
 */
static ethervox_result_t convert_integer(
    conversion_context_t* ctx,
    const char* rule_name
) {
    char rule[256];
    snprintf(rule, sizeof(rule), "%s ::= (\"-\")? [0-9]+\n", rule_name);
    return sb_append(&ctx->output, rule);
}

/**
 * Type 3: Number (float)
 * {"type": "number"} → rule ::= "-"? [0-9]+ ("." [0-9]+)? (("e" | "E") ("-" | "+")? [0-9]+)?
 */
static ethervox_result_t convert_number(
    conversion_context_t* ctx,
    const char* rule_name
) {
    char rule[512];
    // Support integer, decimal, and scientific notation
    snprintf(rule, sizeof(rule),
        "%s ::= (\"-\")? [0-9]+ (\".\" [0-9]+)? ((\"e\" | \"E\") (\"-\" | \"+\")? [0-9]+)?\n",
        rule_name);
    return sb_append(&ctx->output, rule);
}

/**
 * Helper: Escape a string for use in GBNF quoted literal
 * Handles: " → \", \ → \\, newline → \n, tab → \t
 */
static char* escape_for_gbnf(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    size_t capacity = len * 2 + 1;  // Worst case: every char needs escaping
    char* escaped = (char*)malloc(capacity);
    if (!escaped) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        switch (c) {
            case '"':
                escaped[j++] = '\\';
                escaped[j++] = '"';
                break;
            case '\\':
                escaped[j++] = '\\';
                escaped[j++] = '\\';
                break;
            case '\n':
                escaped[j++] = '\\';
                escaped[j++] = 'n';
                break;
            case '\t':
                escaped[j++] = '\\';
                escaped[j++] = 't';
                break;
            case '\r':
                escaped[j++] = '\\';
                escaped[j++] = 'r';
                break;
            default:
                escaped[j++] = c;
                break;
        }
    }
    escaped[j] = '\0';
    return escaped;
}

/**
 * Type 4: String (plain)
 * {"type": "string"} → rule ::= "\"" [^"\]* "\""
 * Note: Escapes quotes and backslashes in the character class
 */
static ethervox_result_t convert_string_plain(
    conversion_context_t* ctx,
    const char* rule_name
) {
    ethervox_result_t result = emit_whitespace_rule(ctx);
    if (result != ETHERVOX_SUCCESS) return result;
    
    char rule[256];
    // Match llama.cpp's JSON string format: exclude control chars, support common escapes, ws after closing quote
    snprintf(rule, sizeof(rule), "%s ::= \"\\\"\" ([^\"\\\\\\x7F\\x00-\\x1F] | \"\\\\\" [\"\\\\/bfnrt])* \"\\\"\" ws\n", rule_name);
    return sb_append(&ctx->output, rule);
}

/**
 * Type 5: String enum
 * {"type": "string", "enum": ["a", "b"]} → rule ::= "\"a\"" | "\"b\""
 */
static ethervox_result_t convert_string_enum(
    conversion_context_t* ctx,
    const char* rule_name,
    const cJSON* enum_values
) {
    if (!cJSON_IsArray(enum_values) || cJSON_GetArraySize(enum_values) == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    string_builder_t rule_sb;
    ethervox_result_t result = sb_init(&rule_sb, 256);
    if (result != ETHERVOX_SUCCESS) return result;
    
    // Start rule
    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s ::= ", rule_name);
    sb_append(&rule_sb, prefix);
    
    // Enumerate values
    int count = cJSON_GetArraySize(enum_values);
    for (int i = 0; i < count; i++) {
        const cJSON* item = cJSON_GetArrayItem(enum_values, i);
        if (!cJSON_IsString(item)) {
            sb_free(&rule_sb);
            return ETHERVOX_ERROR_INVALID_ARGUMENT;
        }
        
        const char* value = cJSON_GetStringValue(item);
        char* escaped = escape_for_gbnf(value);
        if (!escaped) {
            sb_free(&rule_sb);
            return ETHERVOX_ERROR_OUT_OF_MEMORY;
        }
        
        char quoted[512];
        snprintf(quoted, sizeof(quoted), "\"\\\"%s\\\"\"", escaped);
        free(escaped);
        
        sb_append(&rule_sb, quoted);
        
        if (i < count - 1) {
            sb_append(&rule_sb, " | ");
        }
    }
    
    sb_append(&rule_sb, "\n");
    
    // Append to context output
    result = sb_append(&ctx->output, rule_sb.data);
    sb_free(&rule_sb);
    return result;
}

/**
 * Type 6: String maxLength
 * {"type": "string", "maxLength": 5} → rule ::= "\"" [^"\]{0,5} "\""
 */
static ethervox_result_t convert_string_max_length(
    conversion_context_t* ctx,
    const char* rule_name,
    int max_length
) {
    if (max_length < 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    char rule[512];
    snprintf(rule, sizeof(rule),
        "%s ::= \"\\\"\" ([^\"\\\\\\n] | \"\\\\\" [\"\\\\/bfnrt]){0,%d} \"\\\"\"\n",
        rule_name, max_length);
    return sb_append(&ctx->output, rule);
}

/**
 * Type 7: String pattern (basic character classes only)
 * {"type": "string", "pattern": "[a-z]+"} → converts basic patterns
 * Supports: [a-z], [A-Z], [0-9], +, *, ?, literal characters
 * Does NOT support: full regex, lookahead, backreferences, etc.
 */
static ethervox_result_t convert_string_pattern(
    conversion_context_t* ctx,
    const char* rule_name,
    const char* pattern
) {
    if (!pattern) {
        return ETHERVOX_ERROR_NULL_POINTER;
    }
    
    // For now, convert basic patterns directly
    // TODO: Full pattern parser if needed (out of scope per C2.3a)
    char rule[1024];
    snprintf(rule, sizeof(rule), "%s ::= \"\\\"\" %s \"\\\"\"\n", rule_name, pattern);
    return sb_append(&ctx->output, rule);
}

/**
 * String type dispatcher - handles plain, enum, maxLength, pattern
 */
static ethervox_result_t convert_string(
    conversion_context_t* ctx,
    const cJSON* schema,
    const char* rule_name
) {
    // Check for enum constraint
    const cJSON* enum_field = cJSON_GetObjectItem(schema, "enum");
    if (enum_field && cJSON_IsArray(enum_field)) {
        return convert_string_enum(ctx, rule_name, enum_field);
    }
    
    // Check for maxLength constraint
    const cJSON* max_length_field = cJSON_GetObjectItem(schema, "maxLength");
    if (max_length_field && cJSON_IsNumber(max_length_field)) {
        int max_length = cJSON_GetNumberValue(max_length_field);
        return convert_string_max_length(ctx, rule_name, max_length);
    }
    
    // Check for pattern constraint
    const cJSON* pattern_field = cJSON_GetObjectItem(schema, "pattern");
    if (pattern_field && cJSON_IsString(pattern_field)) {
        const char* pattern = cJSON_GetStringValue(pattern_field);
        return convert_string_pattern(ctx, rule_name, pattern);
    }
    
    // Plain string (no constraints)
    return convert_string_plain(ctx, rule_name);
}

/**
 * Helper: Generate whitespace rule (used by objects/arrays)
 */
static ethervox_result_t emit_whitespace_rule(conversion_context_t* ctx) {
    // Only emit once
    if (ctx->ws_emitted) return ETHERVOX_SUCCESS;
    ctx->ws_emitted = 1;
    
    return sb_append(&ctx->helper_rules, "ws ::= | \" \" | \"\\n\" [ \\t]{0,20}\n");
}

/**
 * Helper: Build a GBNF-safe rule-name segment from an arbitrary JSON property name.
 *
 * llama.cpp's own GBNF parser (`is_word_char`, llama-grammar.cpp) accepts only
 * `[A-Za-z0-9-]` in identifiers -- notably NOT `_`. JSON Schema property names routinely
 * contain underscores (`item_id`, `max_chars`, ...), and this converter was embedding them
 * verbatim into generated rule names (`convert_object`'s `value_rule_name`), producing GBNF
 * llama.cpp's own parser then rejects with "expecting ::= at _id-value ::= ..." -- the name
 * parser silently stops at the first `_`, leaving the rest dangling. Only the *rule name* needs
 * this; the literal JSON key text emitted into the grammar (`"\"item_id\""`) is unaffected and
 * must stay exactly as the schema declares it, since that's matched against real JSON output.
 */
static void sanitize_rule_name_segment(const char* name, char* out, size_t out_size) {
    size_t i = 0;
    for (; name[i] != '\0' && i + 1 < out_size; i++) {
        out[i] = (name[i] == '_') ? '-' : name[i];
    }
    out[i] = '\0';
}

/**
 * Type 8-10: Object types
 * Type 8: Empty object {"type": "object", "properties": {}}
 * Type 9: Required fields
 * Type 10: Optional fields
 */
static ethervox_result_t convert_object(
    conversion_context_t* ctx,
    const cJSON* schema,
    const char* rule_name
) {
    ethervox_result_t result;
    
    // Emit whitespace rule if not already emitted
    result = emit_whitespace_rule(ctx);
    if (result != ETHERVOX_SUCCESS) return result;
    
    const cJSON* properties = cJSON_GetObjectItem(schema, "properties");
    const cJSON* required = cJSON_GetObjectItem(schema, "required");
    
    // Type 8: Empty object (no properties or empty properties)
    if (!properties || cJSON_GetArraySize(properties) == 0) {
        char rule[256];
        snprintf(rule, sizeof(rule), "%s ::= \"{\" ws \"}\" ws\n", rule_name);
        return sb_append(&ctx->output, rule);
    }
    
    // Build list of required field names
    int num_required = 0;
    const char* required_fields[64] = {0};
    if (required && cJSON_IsArray(required)) {
        num_required = cJSON_GetArraySize(required);
        for (int i = 0; i < num_required && i < 64; i++) {
            const cJSON* item = cJSON_GetArrayItem(required, i);
            if (cJSON_IsString(item)) {
                required_fields[i] = cJSON_GetStringValue(item);
            }
        }
    }
    
    // Pass 1: convert every property's value schema and build its "key": value member pattern.
    // Members are emitted in `properties` order (the grammar fixes key order, so callers that care
    // -- e.g. a tool-call envelope wanting "name" before "arguments" -- control it by authoring
    // order; BACKLOG-74).
    enum { MAX_OBJECT_PROPS = 64 };
    char members[MAX_OBJECT_PROPS][512];
    int member_required[MAX_OBJECT_PROPS] = {0};
    int prop_count = 0;
    const cJSON* prop = NULL;
    cJSON_ArrayForEach(prop, properties) {
        const char* prop_name = prop->string;
        if (!prop_name) continue;
        if (prop_count >= MAX_OBJECT_PROPS) return ETHERVOX_ERROR_INVALID_ARGUMENT;

        for (int i = 0; i < num_required; i++) {
            if (required_fields[i] && strcmp(required_fields[i], prop_name) == 0) {
                member_required[prop_count] = 1;
                break;
            }
        }

        char safe_prop_name[128];
        sanitize_rule_name_segment(prop_name, safe_prop_name, sizeof(safe_prop_name));
        char value_rule_name[128];
        int vn = snprintf(value_rule_name, sizeof(value_rule_name), "%s-%s-value", rule_name, safe_prop_name);
        if (vn < 0 || (size_t)vn >= sizeof(value_rule_name)) return ETHERVOX_ERROR_BUFFER_TOO_SMALL;

        result = convert_schema_to_gbnf(ctx, prop, value_rule_name);
        if (result != ETHERVOX_SUCCESS) return result;

        int mn = snprintf(members[prop_count], sizeof(members[prop_count]),
                          "\"\\\"%s\\\"\" ws \":\" ws %s", prop_name, value_rule_name);
        if (mn < 0 || (size_t)mn >= sizeof(members[prop_count])) return ETHERVOX_ERROR_BUFFER_TOO_SMALL;
        prop_count++;
    }

    // Pass 2: assemble. A comma belongs to the member that FOLLOWS another member, so it must sit
    // inside that member's optional group -- a comma emitted unconditionally between members
    // forces a stray one whenever an optional member is omitted (BACKLOG-74).
    int first_required = -1;
    for (int i = 0; i < prop_count; i++) {
        if (member_required[i]) { first_required = i; break; }
    }

    string_builder_t obj_sb;
    result = sb_init(&obj_sb, 1024);
    if (result != ETHERVOX_SUCCESS) return result;

    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s ::= \"{\" ws ", rule_name);
    sb_append(&obj_sb, prefix);

    if (first_required >= 0) {
        // Optional members before the first required one carry their trailing comma with them.
        for (int i = 0; i < first_required; i++) {
            sb_append(&obj_sb, "(");
            sb_append(&obj_sb, members[i]);
            sb_append(&obj_sb, " ws \",\" ws) ");
        }
        sb_append(&obj_sb, members[first_required]);
        for (int i = first_required + 1; i < prop_count; i++) {
            if (member_required[i]) {
                sb_append(&obj_sb, " ws \",\" ws ");
                sb_append(&obj_sb, members[i]);
            } else {
                sb_append(&obj_sb, " (ws \",\" ws ");
                sb_append(&obj_sb, members[i]);
                sb_append(&obj_sb, ")?");
            }
        }
    } else {
        // Every member optional: any subset, in order, with commas only between present members.
        sb_append(&obj_sb, "(");
        for (int k = 0; k < prop_count; k++) {
            if (k > 0) sb_append(&obj_sb, " | ");
            sb_append(&obj_sb, members[k]);
            for (int j = k + 1; j < prop_count; j++) {
                sb_append(&obj_sb, " (ws \",\" ws ");
                sb_append(&obj_sb, members[j]);
                sb_append(&obj_sb, ")?");
            }
        }
        sb_append(&obj_sb, ")?");
    }

    sb_append(&obj_sb, " ws \"}\" ws\n");
    
    // Append to context output
    result = sb_append(&ctx->output, obj_sb.data);
    sb_free(&obj_sb);
    return result;
}

/**
 * Type 11-12: Array types
 * Type 11: Array with items schema
 * Type 12: Array with maxItems constraint
 */
static ethervox_result_t convert_array(
    conversion_context_t* ctx,
    const cJSON* schema,
    const char* rule_name
) {
    ethervox_result_t result;
    
    // Emit whitespace rule if not already emitted
    result = emit_whitespace_rule(ctx);
    if (result != ETHERVOX_SUCCESS) return result;
    
    const cJSON* items = cJSON_GetObjectItem(schema, "items");
    if (!items) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;  // Array must have items schema
    }
    
    // Generate subrule for array item
    char item_rule_name[128];
    snprintf(item_rule_name, sizeof(item_rule_name), "%s-item", rule_name);
    
    result = convert_schema_to_gbnf(ctx, items, item_rule_name);
    if (result != ETHERVOX_SUCCESS) return result;
    
    // Check for maxItems constraint
    const cJSON* max_items_field = cJSON_GetObjectItem(schema, "maxItems");
    int max_items = -1;
    if (max_items_field && cJSON_IsNumber(max_items_field)) {
        max_items = cJSON_GetNumberValue(max_items_field);
    }
    
    // Build array rule
    string_builder_t arr_sb;
    result = sb_init(&arr_sb, 512);
    if (result != ETHERVOX_SUCCESS) return result;
    
    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s ::= \"[\" ws ", rule_name);
    sb_append(&arr_sb, prefix);
    
    if (max_items > 0) {
        // Array with maxItems: enumerate all possible lengths
        // [item, item, item] where length <= maxItems
        char pattern[512];
        snprintf(pattern, sizeof(pattern), 
            "(%s (ws \",\" ws %s){0,%d})? ws \"]\"",
            item_rule_name, item_rule_name, max_items - 1);
        sb_append(&arr_sb, pattern);
    } else {
        // Unbounded array: item (, item)*
        char pattern[256];
        snprintf(pattern, sizeof(pattern),
            "(%s (ws \",\" ws %s)*)? ws \"]\"",
            item_rule_name, item_rule_name);
        sb_append(&arr_sb, pattern);
    }
    
    sb_append(&arr_sb, "\n");
    
    result = sb_append(&ctx->output, arr_sb.data);
    sb_free(&arr_sb);
    return result;
}

/**
 * Type 13: oneOf (union types)
 * Generates alternation: variant1 | variant2 | variant3
 */
static ethervox_result_t convert_one_of(
    conversion_context_t* ctx,
    const cJSON* schema,
    const char* rule_name
) {
    ethervox_result_t result;
    
    const cJSON* one_of = cJSON_GetObjectItem(schema, "oneOf");
    if (!one_of || !cJSON_IsArray(one_of)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }
    
    int variant_count = cJSON_GetArraySize(one_of);
    if (variant_count == 0) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;  // Empty oneOf
    }
    
    // Generate subrule for each variant
    string_builder_t union_sb;
    result = sb_init(&union_sb, 512);
    if (result != ETHERVOX_SUCCESS) return result;
    
    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s ::= ", rule_name);
    sb_append(&union_sb, prefix);
    
    for (int i = 0; i < variant_count; i++) {
        const cJSON* variant = cJSON_GetArrayItem(one_of, i);
        if (!variant) continue;
        
        // Generate unique rule name for this variant
        char variant_rule[128];
        snprintf(variant_rule, sizeof(variant_rule), "%s-variant%d", rule_name, i);
        
        // Convert variant schema
        result = convert_schema_to_gbnf(ctx, variant, variant_rule);
        if (result != ETHERVOX_SUCCESS) {
            sb_free(&union_sb);
            return result;
        }
        
        // Add to union (with | separator except for first)
        if (i > 0) {
            sb_append(&union_sb, " | ");
        }
        sb_append(&union_sb, variant_rule);
    }
    
    sb_append(&union_sb, "\n");
    
    result = sb_append(&ctx->output, union_sb.data);
    sb_free(&union_sb);
    return result;
}

// =============================================================================
// Schema type dispatcher
// =============================================================================

static ethervox_result_t convert_schema_to_gbnf(
    conversion_context_t* ctx,
    const cJSON* schema,
    const char* rule_name
) {
    if (!schema) return ETHERVOX_ERROR_NULL_POINTER;
    
    // Check for oneOf first (union types)
    const cJSON* one_of = cJSON_GetObjectItem(schema, "oneOf");
    if (one_of && cJSON_IsArray(one_of)) {
        // Type 13: oneOf (union type)
        return convert_one_of(ctx, schema, rule_name);
    }
    
    // Get type field
    const cJSON* type_field = cJSON_GetObjectItem(schema, "type");
    if (!type_field || !cJSON_IsString(type_field)) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;  // Schema must have "type" or "oneOf"
    }
    
    const char* type = cJSON_GetStringValue(type_field);
    
    if (strcmp(type, "boolean") == 0) {
        return convert_boolean(ctx, rule_name);
    } else if (strcmp(type, "integer") == 0) {
        return convert_integer(ctx, rule_name);
    } else if (strcmp(type, "number") == 0) {
        return convert_number(ctx, rule_name);
    } else if (strcmp(type, "string") == 0) {
        return convert_string(ctx, schema, rule_name);
    } else if (strcmp(type, "object") == 0) {
        return convert_object(ctx, schema, rule_name);
    } else if (strcmp(type, "array") == 0) {
        return convert_array(ctx, schema, rule_name);
    }
    
    // Unsupported type (will be implemented in later iterations)
    return ETHERVOX_ERROR_NOT_IMPLEMENTED;
}

// =============================================================================
// Public API implementation
// =============================================================================

ethervox_result_t ethervox_grammar_compile(
    const char* gbnf_source,
    const char* root_rule,
    ethervox_grammar_t** out_grammar
) {
    if (!gbnf_source || !root_rule || !out_grammar) {
        return ETHERVOX_ERROR_NULL_POINTER;
    }
    
    ethervox_grammar_t* grammar = (ethervox_grammar_t*)calloc(1, sizeof(ethervox_grammar_t));
    if (!grammar) return ETHERVOX_ERROR_OUT_OF_MEMORY;
    
    grammar->gbnf_source = strdup(gbnf_source);
    grammar->root_rule = strdup(root_rule);
    grammar->is_lazy = false;  // Immediate mode by default
    grammar->trigger_words = NULL;
    grammar->trigger_word_count = 0;
    
    if (!grammar->gbnf_source || !grammar->root_rule) {
        ethervox_grammar_free(grammar);
        return ETHERVOX_ERROR_OUT_OF_MEMORY;
    }
    
    *out_grammar = grammar;
    return ETHERVOX_SUCCESS;
}

ethervox_result_t ethervox_grammar_from_json_schema(
    const char* json_schema,
    ethervox_grammar_t** out_grammar
) {
    if (!json_schema || !out_grammar) {
        return ETHERVOX_ERROR_NULL_POINTER;
    }
    
    // Parse JSON schema
    cJSON* schema = cJSON_Parse(json_schema);
    if (!schema) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;  // Malformed JSON
    }
    
    // Initialize conversion context
    conversion_context_t ctx;
    ctx.rule_counter = 0;
    ctx.ws_emitted = 0;
    ethervox_result_t result = sb_init(&ctx.output, 1024);
    if (result != ETHERVOX_SUCCESS) {
        cJSON_Delete(schema);
        return result;
    }
    result = sb_init(&ctx.helper_rules, 256);
    if (result != ETHERVOX_SUCCESS) {
        sb_free(&ctx.output);
        cJSON_Delete(schema);
        return result;
    }
    
    // Convert schema to GBNF starting with "root" rule
    result = convert_schema_to_gbnf(&ctx, schema, "root");
    
    cJSON_Delete(schema);
    
    if (result != ETHERVOX_SUCCESS) {
        sb_free(&ctx.output);
        sb_free(&ctx.helper_rules);
        return result;
    }
    
    // Append helper rules at the end
    if (ctx.helper_rules.length > 0) {
        result = sb_append(&ctx.output, ctx.helper_rules.data);
        if (result != ETHERVOX_SUCCESS) {
            sb_free(&ctx.output);
            sb_free(&ctx.helper_rules);
            return result;
        }
    }
    
    // Create grammar from generated GBNF
    result = ethervox_grammar_compile(ctx.output.data, "root", out_grammar);
    
    sb_free(&ctx.output);
    sb_free(&ctx.helper_rules);
    return result;
}

void ethervox_grammar_free(ethervox_grammar_t* grammar) {
    if (!grammar) return;
    
    free(grammar->gbnf_source);
    free(grammar->root_rule);
    
    // Free trigger words array
    if (grammar->trigger_words) {
        for (size_t i = 0; i < grammar->trigger_word_count; i++) {
            free(grammar->trigger_words[i]);
        }
        free(grammar->trigger_words);
    }
    
    free(grammar);
}

const char* ethervox_grammar_get_source(const ethervox_grammar_t* grammar) {
    return grammar ? grammar->gbnf_source : NULL;
}

const char* ethervox_grammar_get_root(const ethervox_grammar_t* grammar) {
    return grammar ? grammar->root_rule : NULL;
}

bool ethervox_grammar_is_lazy(const ethervox_grammar_t* grammar) {
    return grammar ? grammar->is_lazy : false;
}

const char* const* ethervox_grammar_get_trigger_words(
    const ethervox_grammar_t* grammar,
    size_t* out_count
) {
    if (!grammar || !grammar->is_lazy) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    
    if (out_count) *out_count = grammar->trigger_word_count;
    return (const char* const*)grammar->trigger_words;
}

ethervox_result_t ethervox_grammar_set_lazy_mode(
    ethervox_grammar_t* grammar,
    const char** trigger_words,
    size_t trigger_word_count
) {
    if (!grammar) return ETHERVOX_ERROR_NULL_POINTER;
    
    // Free existing trigger words if any
    if (grammar->trigger_words) {
        for (size_t i = 0; i < grammar->trigger_word_count; i++) {
            free(grammar->trigger_words[i]);
        }
        free(grammar->trigger_words);
        grammar->trigger_words = NULL;
        grammar->trigger_word_count = 0;
    }
    
    // Disable lazy mode if no triggers provided
    if (!trigger_words || trigger_word_count == 0) {
        grammar->is_lazy = false;
        return ETHERVOX_SUCCESS;
    }
    
    // Copy trigger words
    grammar->trigger_words = (char**)malloc(trigger_word_count * sizeof(char*));
    if (!grammar->trigger_words) return ETHERVOX_ERROR_OUT_OF_MEMORY;
    
    for (size_t i = 0; i < trigger_word_count; i++) {
        grammar->trigger_words[i] = strdup(trigger_words[i]);
        if (!grammar->trigger_words[i]) {
            // Clean up on failure
            for (size_t j = 0; j < i; j++) {
                free(grammar->trigger_words[j]);
            }
            free(grammar->trigger_words);
            grammar->trigger_words = NULL;
            return ETHERVOX_ERROR_OUT_OF_MEMORY;
        }
    }
    
    grammar->trigger_word_count = trigger_word_count;
    grammar->is_lazy = true;
    
    return ETHERVOX_SUCCESS;
}
