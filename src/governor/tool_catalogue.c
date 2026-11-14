/**
 * @file tool_catalogue.c
 * @brief Load tool contracts from embedded JSON (TASK-C2.6a)
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#include "ethervox/tool_catalogue.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

// Every profile name a contract's "profiles" array may legally contain. An entry
// outside this list is a data-authoring error (16-TOOLS.md §2: "declare profiles
// honestly"), not something to silently ignore.
static bool is_known_profile_name(const char* name) {
    return strcmp(name, "EDGE") == 0 ||
           strcmp(name, "MOBILE") == 0 ||
           strcmp(name, "DESKTOP") == 0 ||
           strcmp(name, "WORKSPACE") == 0;
}

ethervox_result_t ethervox_tool_catalogue_load(const char* catalogue_json,
                                               const char* tool_name,
                                               const char* build_profile,
                                               ethervox_tool_t* tool) {
    ETHERVOX_CHECK_PTR(catalogue_json);
    ETHERVOX_CHECK_PTR(tool_name);
    ETHERVOX_CHECK_PTR(build_profile);
    ETHERVOX_CHECK_PTR(tool);

    cJSON* root = cJSON_Parse(catalogue_json);
    if (!root || !cJSON_IsArray(root)) {
        cJSON_Delete(root);
        ETHERVOX_RETURN_ERROR(ETHERVOX_ERROR_INVALID_ARGUMENT, "Catalogue JSON is not an array");
    }

    ethervox_result_t status = ETHERVOX_ERROR_NOT_FOUND;
    cJSON* contract;
    cJSON_ArrayForEach(contract, root) {
        cJSON* name = cJSON_GetObjectItemCaseSensitive(contract, "name");
        if (!cJSON_IsString(name) || strcmp(name->valuestring, tool_name) != 0) {
            continue;
        }

        // Found the contract. Validate its declared profiles before touching *tool -
        // a partially-filled tool on error would be worse than none.
        cJSON* profiles = cJSON_GetObjectItemCaseSensitive(contract, "profiles");
        bool profile_list_ok = cJSON_IsArray(profiles);
        bool build_profile_included = false;
        if (profile_list_ok) {
            cJSON* p;
            cJSON_ArrayForEach(p, profiles) {
                if (!cJSON_IsString(p) || !is_known_profile_name(p->valuestring)) {
                    profile_list_ok = false;
                    break;
                }
                if (strcmp(p->valuestring, build_profile) == 0) {
                    build_profile_included = true;
                }
            }
        }
        if (!profile_list_ok) {
            status = ETHERVOX_ERROR_INVALID_ARGUMENT;
            break;
        }
        if (!build_profile_included) {
            status = ETHERVOX_ERROR_NOT_SUPPORTED;
            break;
        }

        cJSON* description = cJSON_GetObjectItemCaseSensitive(contract, "description");
        cJSON* schema = cJSON_GetObjectItemCaseSensitive(contract, "schema");
        if (!cJSON_IsString(description) || !schema) {
            status = ETHERVOX_ERROR_INVALID_ARGUMENT;
            break;
        }

        char* schema_str = cJSON_PrintUnformatted(schema);
        if (!schema_str) {
            status = ETHERVOX_ERROR_OUT_OF_MEMORY;
            break;
        }

        memset(tool->name, 0, sizeof(tool->name));
        memset(tool->description, 0, sizeof(tool->description));
        memset(tool->parameters_json_schema, 0, sizeof(tool->parameters_json_schema));
        strncpy(tool->name, name->valuestring, sizeof(tool->name) - 1);
        strncpy(tool->description, description->valuestring, sizeof(tool->description) - 1);
        strncpy(tool->parameters_json_schema, schema_str, sizeof(tool->parameters_json_schema) - 1);

        free(schema_str);
        status = ETHERVOX_SUCCESS;
        break;
    }

    cJSON_Delete(root);
    return status;
}
