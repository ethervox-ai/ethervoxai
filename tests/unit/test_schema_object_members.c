// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
/**
 * @file test_schema_object_members.c
 * @brief Object member ordering and comma placement in JSON-Schema -> GBNF (BACKLOG-74)
 *
 * convert_object used to emit the comma between members unconditionally, so an optional member
 * produced a mandatory comma: `a "," (b)?` accepts `{"a":1,}` and rejects `{"a":1}`. The comma
 * must live inside the optional group of the member that FOLLOWS another member.
 */

#include "ethervox/grammar.h"
#include "test_utils.h"
#include <stdlib.h>
#include <string.h>

static char* root_rule(const char* schema) {
    ethervox_grammar_t* g = NULL;
    CHECK(ethervox_grammar_from_json_schema(schema, &g) == ETHERVOX_SUCCESS);
    const char* src = ethervox_grammar_get_source(g);
    const char* line = strstr(src, "root ::=");
    CHECK(line != NULL);
    const char* end = strchr(line, '\n');
    size_t n = end ? (size_t)(end - line) : strlen(line);
    char* out = malloc(n + 1);
    memcpy(out, line, n);
    out[n] = '\0';
    ethervox_grammar_free(g);
    return out;
}

#define MEMBER(k) "\"\\\"" k "\\\"\" ws \":\" ws root-" k "-value"

static void test_required_members_keep_their_commas_and_authored_order(void) {
    char* r = root_rule("{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},"
                        "\"arguments\":{\"type\":\"string\"}},\"required\":[\"name\",\"arguments\"]}");
    const char* expected = "root ::= \"{\" ws " MEMBER("name") " ws \",\" ws " MEMBER("arguments") " ws \"}\" ws";
    CHECK(strcmp(r, expected) == 0);
    free(r);
    printf("  ✓ authored order is kept: name before arguments\n");
}

static void test_optional_middle_member_owns_its_comma(void) {
    char* r = root_rule("{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"string\"},"
                        "\"b\":{\"type\":\"string\"},\"c\":{\"type\":\"string\"}},\"required\":[\"a\",\"c\"]}");
    const char* expected = "root ::= \"{\" ws " MEMBER("a") " (ws \",\" ws " MEMBER("b") ")?"
                           " ws \",\" ws " MEMBER("c") " ws \"}\" ws";
    CHECK(strcmp(r, expected) == 0);
    free(r);
    printf("  ✓ optional middle member: no stray comma when omitted\n");
}

static void test_optional_last_member_owns_its_comma(void) {
    char* r = root_rule("{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"string\"},"
                        "\"b\":{\"type\":\"string\"}},\"required\":[\"a\"]}");
    const char* expected = "root ::= \"{\" ws " MEMBER("a") " (ws \",\" ws " MEMBER("b") ")? ws \"}\" ws";
    CHECK(strcmp(r, expected) == 0);
    free(r);
    printf("  ✓ optional last member: no trailing comma when omitted\n");
}

static void test_optional_first_member_carries_its_trailing_comma(void) {
    char* r = root_rule("{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"string\"},"
                        "\"b\":{\"type\":\"string\"}},\"required\":[\"b\"]}");
    const char* expected = "root ::= \"{\" ws (" MEMBER("a") " ws \",\" ws) " MEMBER("b") " ws \"}\" ws";
    CHECK(strcmp(r, expected) == 0);
    free(r);
    printf("  ✓ optional first member: no leading comma when omitted\n");
}

static void test_all_optional_members_accept_any_ordered_subset(void) {
    char* r = root_rule("{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"string\"},"
                        "\"b\":{\"type\":\"string\"}}}");
    const char* expected = "root ::= \"{\" ws (" MEMBER("a") " (ws \",\" ws " MEMBER("b") ")?"
                           " | " MEMBER("b") ")? ws \"}\" ws";
    CHECK(strcmp(r, expected) == 0);
    free(r);
    printf("  ✓ all-optional object: any ordered subset, commas only between present members\n");
}

int main(void) {
    printf("=== schema object member order / commas (BACKLOG-74) ===\n");
    test_required_members_keep_their_commas_and_authored_order();
    test_optional_middle_member_owns_its_comma();
    test_optional_last_member_owns_its_comma();
    test_optional_first_member_carries_its_trailing_comma();
    test_all_optional_members_accept_any_ordered_subset();
    printf("ALL PASSED\n");
    return 0;
}
