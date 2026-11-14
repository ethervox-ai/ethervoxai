/**
 * @file tool_catalogue.h
 * @brief Load tool contracts (name, description, schema, profiles) from JSON
 *
 * Part of TASK-C2.6a: tool contracts are data (16-TOOLS.md), not C literals, so the
 * same JSON drives every product and cannot drift between platforms. This loader
 * fills in only the *contract* fields of `ethervox_tool_t` (name, description,
 * parameters_json_schema); the caller still supplies `execute`, `test_scenario` and
 * the deterministic/confirmation/stateful/latency flags, since a function pointer
 * cannot be data.
 *
 * The JSON itself is never read from disk at runtime (no I/O, works sandboxed and on
 * ESP32) - it is embedded into the binary at build time as a generated C string; see
 * `tools/catalogue/CMakeLists.txt`.
 *
 * Copyright (c) 2024-2026 EthervoxAI Team
 * SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary
 */

#ifndef ETHERVOX_TOOL_CATALOGUE_H
#define ETHERVOX_TOOL_CATALOGUE_H

#include "ethervox/governor.h"
#include "ethervox/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Fill in a tool's contract fields (name, description, parameters_json_schema) from
 * an embedded catalogue JSON array, by tool name.
 *
 * Only `name`, `description` and `parameters_json_schema` are written. All other
 * fields of `*tool` (execute, test_scenario, is_deterministic, requires_confirmation,
 * is_stateful, estimated_latency_ms) are left untouched - set them before or after
 * calling this.
 *
 * @param catalogue_json Embedded catalogue JSON array (see tools/catalogue directory)
 * @param tool_name       Contract to look up (must match a "name" in the catalogue)
 * @param build_profile   This build's profile, one of "EDGE"/"MOBILE"/"DESKTOP"/"WORKSPACE"
 * @param tool            Output: contract fields written into this tool
 * @return ETHERVOX_SUCCESS, ETHERVOX_ERROR_NOT_FOUND if no contract has that name,
 *         ETHERVOX_ERROR_NOT_SUPPORTED if the contract exists but excludes
 *         build_profile, or ETHERVOX_ERROR_INVALID_ARGUMENT on malformed JSON or an
 *         unrecognised profile name in the contract.
 */
ethervox_result_t ethervox_tool_catalogue_load(const char* catalogue_json,
                                               const char* tool_name,
                                               const char* build_profile,
                                               ethervox_tool_t* tool);

/**
 * This build's profile name, one of "EDGE"/"MOBILE"/"DESKTOP"/"WORKSPACE", matching
 * the `ETHERVOX_PROFILE_*` macro CMake defined (C1.2). Falls back to "DESKTOP" if
 * none is defined, matching the rest of the codebase's default-profile behaviour.
 */
static inline const char* ethervox_tool_catalogue_build_profile(void) {
#if defined(ETHERVOX_PROFILE_EDGE)
    return "EDGE";
#elif defined(ETHERVOX_PROFILE_MOBILE)
    return "MOBILE";
#elif defined(ETHERVOX_PROFILE_WORKSPACE)
    return "WORKSPACE";
#else
    return "DESKTOP";
#endif
}

#ifdef __cplusplus
}
#endif

#endif // ETHERVOX_TOOL_CATALOGUE_H
