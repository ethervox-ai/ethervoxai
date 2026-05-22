/**
 * @file workspace_tools.c
 * @brief Workspace tools plugin for EthervoxAI
 *
 * Provides LLM access to workspace operations including:
 * - Listing and searching objects
 * - Reading object content
 * - Creating notes and connections
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include "workspace_operations.h"

// Global workspace operations (set during registration)
static workspace_operations_t* g_workspace_ops = NULL;

/**
 * @brief Parse a JSON string parameter from args_json
 *
 * Simple JSON parser for extracting string values from {"key": "value"} format
 */
static int parse_json_string(const char* json, const char* key, char* out, size_t out_size) {
  if (!json || !key || !out)
    return -1;

  char search_key[256];
  snprintf(search_key, sizeof(search_key), "\"%s\":", key);

  const char* start = strstr(json, search_key);
  if (!start) {
    // Try with space after colon
    snprintf(search_key, sizeof(search_key), "\"%s\": ", key);
    start = strstr(json, search_key);
    if (!start)
      return -1;
  }

  start += strlen(search_key);
  while (*start == ' ' || *start == '\t')
    start++;

  if (*start != '\"')
    return -1;
  start++;

  const char* end = strchr(start, '\"');
  if (!end)
    return -1;

  size_t len = end - start;
  if (len >= out_size)
    len = out_size - 1;

  memcpy(out, start, len);
  out[len] = '\0';
  return 0;
}

//=============================================================================
// Tool Wrapper Functions
//=============================================================================

/**
 * @brief Tool wrapper: workspace_list_objects
 */
static int tool_workspace_list_objects_wrapper(const char* args_json, char** result, char** error) {
  if (!g_workspace_ops || !g_workspace_ops->list_objects) {
    *error = strdup("Workspace operations not initialized");
    return -1;
  }

  char type_filter[64] = "all";

  // Try to parse type filter (optional parameter)
  if (args_json && *args_json != '\0') {
    parse_json_string(args_json, "type", type_filter, sizeof(type_filter));
  }

  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
               "Listing workspace objects with filter: %s", type_filter);

  return g_workspace_ops->list_objects(type_filter, result, error);
}

/**
 * @brief Tool wrapper: workspace_search_objects
 */
static int tool_workspace_search_objects_wrapper(const char* args_json, char** result,
                                                 char** error) {
  if (!g_workspace_ops || !g_workspace_ops->search_objects) {
    *error = strdup("Workspace operations not initialized");
    return -1;
  }

  char query[1024];

  if (parse_json_string(args_json, "query", query, sizeof(query)) != 0) {
    *error = strdup("Missing required parameter: 'query'");
    return -1;
  }

  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, "Searching workspace for: %s",
               query);

  return g_workspace_ops->search_objects(query, result, error);
}

/**
 * @brief Tool wrapper: workspace_get_object
 */
static int tool_workspace_get_object_wrapper(const char* args_json, char** result, char** error) {
  if (!g_workspace_ops || !g_workspace_ops->get_object) {
    *error = strdup("Workspace operations not initialized");
    return -1;
  }

  char object_id[64];

  if (parse_json_string(args_json, "object_id", object_id, sizeof(object_id)) != 0) {
    *error = strdup("Missing required parameter: 'object_id'");
    return -1;
  }

  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, "Getting object: %s",
               object_id);

  return g_workspace_ops->get_object(object_id, result, error);
}

/**
 * @brief Tool wrapper: workspace_create_note
 */
static int tool_workspace_create_note_wrapper(const char* args_json, char** result, char** error) {
  if (!g_workspace_ops || !g_workspace_ops->create_note) {
    *error = strdup("Workspace operations not initialized");
    return -1;
  }

  char title[256];
  char* content = NULL;
  char tags_json[512] = "[]";

  // Parse required parameters
  if (parse_json_string(args_json, "title", title, sizeof(title)) != 0) {
    *error = strdup("Missing required parameter: 'title'");
    return -1;
  }

  // Parse content (can be large, so we need special handling)
  const char* content_key = "\"content\":\"";
  const char* content_start = strstr(args_json, content_key);
  if (!content_start) {
    content_key = "\"content\": \"";
    content_start = strstr(args_json, content_key);
  }

  if (content_start) {
    content_start += strlen(content_key);
    const char* content_end = strstr(content_start, "\",");
    if (!content_end)
      content_end = strstr(content_start, "\"}");

    if (content_end) {
      size_t content_len = content_end - content_start;
      content = (char*)malloc(content_len + 1);
      if (content) {
        memcpy(content, content_start, content_len);
        content[content_len] = '\0';
      }
    }
  }

  if (!content) {
    *error = strdup("Missing required parameter: 'content'");
    return -1;
  }

  // Parse optional tags array
  parse_json_string(args_json, "tags", tags_json, sizeof(tags_json));

  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, "Creating note: %s", title);

  int ret = g_workspace_ops->create_note(title, content, tags_json, result, error);
  free(content);
  return ret;
}

/**
 * @brief Tool wrapper: workspace_create_connection
 */
static int tool_workspace_create_connection_wrapper(const char* args_json, char** result,
                                                    char** error) {
  if (!g_workspace_ops || !g_workspace_ops->create_connection) {
    *error = strdup("Workspace operations not initialized");
    return -1;
  }

  char from_id[64];
  char to_id[64];
  char label[128] = "";

  // Parse required parameters
  if (parse_json_string(args_json, "from_id", from_id, sizeof(from_id)) != 0) {
    *error = strdup("Missing required parameter: 'from_id'");
    return -1;
  }

  if (parse_json_string(args_json, "to_id", to_id, sizeof(to_id)) != 0) {
    *error = strdup("Missing required parameter: 'to_id'");
    return -1;
  }

  // Parse optional label
  parse_json_string(args_json, "label", label, sizeof(label));

  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
               "Creating connection: %s -> %s", from_id, to_id);

  const char* label_param = (label[0] != '\0') ? label : NULL;
  return g_workspace_ops->create_connection(from_id, to_id, label_param, result, error);
}

/**
 * @brief Tool wrapper: workspace_update_object
 */
static int tool_workspace_update_object_wrapper(const char* args_json, char** result,
                                                char** error) {
  if (!g_workspace_ops || !g_workspace_ops->update_object) {
    *error = strdup("Workspace operations not initialized");
    return -1;
  }

  char object_id[64];
  char* content = NULL;
  char append[16] = "true";  // Default to append mode

  // Parse required parameters
  if (parse_json_string(args_json, "object_id", object_id, sizeof(object_id)) != 0) {
    *error = strdup("Missing required parameter: 'object_id'");
    return -1;
  }

  // Parse content (can be large, so we need special handling)
  const char* content_key = "\"content\":\"";
  const char* content_start = strstr(args_json, content_key);
  if (!content_start) {
    content_key = "\"content\": \"";
    content_start = strstr(args_json, content_key);
  }

  if (content_start) {
    content_start += strlen(content_key);
    const char* content_end = strstr(content_start, "\",");
    if (!content_end)
      content_end = strstr(content_start, "\"}");

    if (content_end) {
      size_t content_len = content_end - content_start;
      content = (char*)malloc(content_len + 1);
      if (content) {
        memcpy(content, content_start, content_len);
        content[content_len] = '\0';
      }
    }
  }

  if (!content) {
    *error = strdup("Missing required parameter: 'content'");
    return -1;
  }

  // Parse optional append mode
  parse_json_string(args_json, "append", append, sizeof(append));

  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
               "Updating object: %s (append=%s)", object_id, append);

  int ret = g_workspace_ops->update_object(object_id, content, append, result, error);
  free(content);
  return ret;
}

/**
 * @brief Tool wrapper: workspace_export_to_docx
 */
static int tool_workspace_export_to_docx_wrapper(const char* args_json, char** result,
                                                 char** error) {
  if (!g_workspace_ops || !g_workspace_ops->export_to_docx) {
    *error = strdup("Workspace operations not initialized or export function not available");
    return -1;
  }

  char object_id[64];

  // Parse required object_id parameter
  if (parse_json_string(args_json, "object_id", object_id, sizeof(object_id)) != 0) {
    *error = strdup("Missing required parameter: 'object_id'");
    return -1;
  }

  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
               "Exporting object to DOCX: %s", object_id);

  return g_workspace_ops->export_to_docx(object_id, result, error);
}

/**
 * @brief Tool wrapper: workspace_highlight_nodes
 */
static int tool_workspace_highlight_nodes_wrapper(const char* args_json, char** result,
                                                   char** error) {
  if (!g_workspace_ops || !g_workspace_ops->highlight_nodes) {
    *error = strdup("Workspace operations not initialized");
    return -1;
  }

  // Parse node_ids array from JSON
  // args_json should be: {"node_ids": ["id1", "id2", ...]}
  
  // Extract just the node_ids array JSON string
  // For now, simple extraction - in production you'd use a JSON parser
  const char* node_ids_start = strstr(args_json, "\"node_ids\"");
  if (!node_ids_start) {
    *error = strdup("Missing required parameter: 'node_ids'");
    return -1;
  }
  
  // Find the array value
  const char* array_start = strchr(node_ids_start, '[');
  if (!array_start) {
    *error = strdup("Invalid node_ids format (expected array)");
    return -1;
  }
  
  // Find the matching closing bracket
  int bracket_count = 1;
  const char* p = array_start + 1;
  while (*p && bracket_count > 0) {
    if (*p == '[') bracket_count++;
    else if (*p == ']') bracket_count--;
    p++;
  }
  
  if (bracket_count != 0) {
    *error = strdup("Malformed node_ids array");
    return -1;
  }
  
  // Extract the array substring
  size_t array_len = p - array_start;
  char* node_ids_json = strndup(array_start, array_len);
  
  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
               "Highlighting nodes: %s", node_ids_json);

  int ret = g_workspace_ops->highlight_nodes(node_ids_json, result, error);
  free(node_ids_json);
  return ret;
}

//=============================================================================
// Tool Registration
//=============================================================================

/**
 * @brief Register all workspace tools with the tool registry
 *
 * @param registry Tool registry to register with
 * @param operations Workspace operations callback structure from Rust
 * @return ETHERVOX_SUCCESS on success, error code otherwise
 */
ethervox_result_t ethervox_workspace_tools_register(ethervox_tool_registry_t* registry,
                                                    workspace_operations_t* operations) {
  if (!registry || !operations) {
    ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                 "Invalid parameters for workspace tools registration");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Store operations for tool wrappers to use
  g_workspace_ops = operations;

  ethervox_result_t result;

  // Register: workspace_list_objects
  ethervox_tool_t list_tool = {
      .name = "workspace_list_objects",
      .description =
          "List all objects in the workspace (notes, files, folders). Use when user asks to list, "
          "show, or see objects. Returns JSON array of objects with id, title, type, tags.",
      .parameters_json_schema =
          "{\"type\":{\"type\":\"string\",\"description\":\"Object type filter: markdown-note, "
          "folder, file, code-snippet, web-link, conversation, image, or "
          "all\",\"default\":\"all\"}}",
      .execute = tool_workspace_list_objects_wrapper,
      .requires_confirmation = 0};
  result = ethervox_tool_registry_add(registry, &list_tool);
  if (result != ETHERVOX_SUCCESS) {
    ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                 "Failed to register workspace_list_objects");
    return result;
  }

  // Register: workspace_search_objects
  ethervox_tool_t search_tool = {
      .name = "workspace_search_objects",
      .description =
          "Search workspace for objects by title, content, or tags. Use when user asks to find, "
          "search for, or locate objects. Returns matching objects with similarity ranking.",
      .parameters_json_schema =
          "{\"query\":{\"type\":\"string\",\"description\":\"Search query "
          "text\",\"required\":true}}",
      .execute = tool_workspace_search_objects_wrapper,
      .requires_confirmation = 0};
  result = ethervox_tool_registry_add(registry, &search_tool);
  if (result != ETHERVOX_SUCCESS) {
    ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                 "Failed to register workspace_search_objects");
    return result;
  }

  // Register: workspace_get_object
  ethervox_tool_t get_tool = {
      .name = "workspace_get_object",
      .description =
          "Get complete details of a specific object including content, metadata, connections, and "
          "files. Requires object_id.",
      .parameters_json_schema =
          "{\"object_id\":{\"type\":\"string\",\"description\":\"Object UUID\",\"required\":true}}",
      .execute = tool_workspace_get_object_wrapper,
      .requires_confirmation = 0};
  result = ethervox_tool_registry_add(registry, &get_tool);
  if (result != ETHERVOX_SUCCESS) {
    ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                 "Failed to register workspace_get_object");
    return result;
  }

  // Register: workspace_create_note
  ethervox_tool_t create_note_tool = {
      .name = "workspace_create_note",
      .description =
          "CREATE a new graph node/object in the workspace (stored as a markdown note). Use this "
          "when the user asks to create/add a new 'node', 'object', 'note', or 'item' in the graph. "
          "Parameters: 'title' (string, required) - the node name/title, and 'content' (string, "
          "required) - node content in markdown format. Returns the newly created node's UUID. "
          "IMPORTANT: This creates a NEW standalone graph node. To add info to an EXISTING node, "
          "first search for it with workspace_search_objects, then use workspace_update_object.",
      .parameters_json_schema =
          "{\"title\":{\"type\":\"string\",\"description\":\"Node/object title/name\",\"required\":true},"
          "\"content\":{\"type\":\"string\",\"description\":\"Node content in markdown "
          "format\",\"required\":true},\"tags\":{\"type\":\"array\",\"description\":\"Optional "
          "tags list\",\"required\":false}}",
      .execute = tool_workspace_create_note_wrapper,
      .requires_confirmation = 0};
  result = ethervox_tool_registry_add(registry, &create_note_tool);
  if (result != ETHERVOX_SUCCESS) {
    ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                 "Failed to register workspace_create_note");
    return result;
  }

  // Register: workspace_create_connection
  ethervox_tool_t create_connection_tool = {
      .name = "workspace_create_connection",
      .description =
          "Create a connection (edge) between two objects in the graph. Requires from_id and "
          "to_id. Optional label.",
      .parameters_json_schema =
          "{\"from_id\":{\"type\":\"string\",\"description\":\"Source object "
          "UUID\",\"required\":true},\"to_id\":{\"type\":\"string\",\"description\":\"Target "
          "object "
          "UUID\",\"required\":true},\"label\":{\"type\":\"string\",\"description\":\"Connection "
          "label\"}}",
      .execute = tool_workspace_create_connection_wrapper,
      .requires_confirmation = 0};
  result = ethervox_tool_registry_add(registry, &create_connection_tool);
  if (result != ETHERVOX_SUCCESS) {
    ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                 "Failed to register workspace_create_connection");
    return result;
  }

  // Register: workspace_update_object
  ethervox_tool_t update_object_tool = {
      .name = "workspace_update_object",
      .description =
          "Update or append content to an EXISTING object in the workspace. Use when user wants "
          "to add information to an existing note, person, or object. First use "
          "workspace_search_objects to find the object_id, then call this to update its content. "
          "Parameters: 'object_id' (string, required), 'content' (string, required), 'append' "
          "(string, optional, 'true' or 'false', defaults to 'true'). When append=true, adds new "
          "content after existing content. When append=false, replaces entire content. Returns "
          "updated object details.",
      .parameters_json_schema =
          "{\"object_id\":{\"type\":\"string\",\"description\":\"UUID of object to "
          "update\",\"required\":true},\"content\":{\"type\":\"string\",\"description\":\"New "
          "content to add or replace\",\"required\":true},\"append\":{\"type\":\"string\","
          "\"description\":\"Append (true) or replace (false) "
          "content\",\"default\":\"true\"}}",
      .execute = tool_workspace_update_object_wrapper,
      .requires_confirmation = 0};
  result = ethervox_tool_registry_add(registry, &update_object_tool);
  if (result != ETHERVOX_SUCCESS) {
    ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                 "Failed to register workspace_update_object");
    return result;
  }

  // Register: workspace_export_to_docx
  ethervox_tool_t export_docx_tool = {
      .name = "workspace_export_to_docx",
      .description =
          "Export a markdown note to Microsoft Word (.docx) format. Use this when the user asks "
          "to create a Word document, export to Word, or wants a .docx file. The system stores "
          "everything as markdown internally, but can export to Word format on demand. "
          "Parameters: 'object_id' (string, required). Returns the path to the generated .docx "
          "file. Requires pandoc to be installed on the system.",
      .parameters_json_schema =
          "{\"object_id\":{\"type\":\"string\",\"description\":\"UUID of the markdown note to "
          "export\",\"required\":true}}",
      .execute = tool_workspace_export_to_docx_wrapper,
      .requires_confirmation = 0};
  result = ethervox_tool_registry_add(registry, &export_docx_tool);
  if (result != ETHERVOX_SUCCESS) {
    ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                 "Failed to register workspace_export_to_docx");
    return result;
  }

  // Register: workspace_highlight_nodes
  ethervox_tool_t highlight_tool = {
      .name = "workspace_highlight_nodes",
      .description =
          "Highlight specific nodes in the graph visualization. Use this when the user searches "
          "for objects, asks to find or show specific items, or wants visual emphasis on certain "
          "nodes. This tool takes an array of node IDs and highlights them in the UI so the user "
          "can see them clearly. Combine with workspace_search_objects to first find nodes, then "
          "highlight the results. Parameters: 'node_ids' (array of strings, required). Returns "
          "success status and the count of highlighted nodes.",
      .parameters_json_schema =
          "{\"node_ids\":{\"type\":\"array\",\"description\":\"Array of node UUIDs to highlight "
          "in the graph\",\"items\":{\"type\":\"string\"},\"required\":true}}",
      .execute = tool_workspace_highlight_nodes_wrapper,
      .requires_confirmation = 0};
  result = ethervox_tool_registry_add(registry, &highlight_tool);
  if (result != ETHERVOX_SUCCESS) {
    ethervox_log(ETHERVOX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__,
                 "Failed to register workspace_highlight_nodes");
    return result;
  }

  ethervox_log(ETHERVOX_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
               "Registered 8 workspace tools successfully");

  return ETHERVOX_SUCCESS;
}
