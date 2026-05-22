/**
 * @file workspace_operations.h
 * @brief Function pointer definitions for workspace operations (implemented in Rust)
 *
 * This header defines the interface between the C workspace tools plugin and
 * the Rust implementation. Function pointers are passed from Rust during
 * initialization to enable the C tools to call back into Rust workspace logic.
 */

#ifndef ETHERVOX_WORKSPACE_OPERATIONS_H
#define ETHERVOX_WORKSPACE_OPERATIONS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief List all objects in the workspace
 *
 * @param type_filter Optional type filter (e.g., "markdown-note", "folder", "all")
 * @param result Output parameter for JSON array of objects
 * @param error Output parameter for error messages
 * @return 0 on success, -1 on error
 */
typedef int (*workspace_list_objects_fn)(const char* type_filter, char** result, char** error);

/**
 * @brief Search workspace objects by query
 *
 * @param query Search query string
 * @param result Output parameter for JSON array of matching objects
 * @param error Output parameter for error messages
 * @return 0 on success, -1 on error
 */
typedef int (*workspace_search_objects_fn)(const char* query, char** result, char** error);

/**
 * @brief Get complete details of a specific object
 *
 * @param object_id UUID of the object to retrieve
 * @param result Output parameter for JSON object with full details
 * @param error Output parameter for error messages
 * @return 0 on success, -1 on error
 */
typedef int (*workspace_get_object_fn)(const char* object_id, char** result, char** error);

/**
 * @brief Create a new markdown note
 *
 * @param title Title of the note
 * @param content Markdown content
 * @param tags_json JSON array of tags (optional, can be NULL)
 * @param result Output parameter for JSON object with created note details
 * @param error Output parameter for error messages
 * @return 0 on success, -1 on error
 */
typedef int (*workspace_create_note_fn)(const char* title, const char* content,
                                        const char* tags_json, char** result, char** error);

/**
 * @brief Create a connection between two objects
 *
 * @param from_id Source object UUID
 * @param to_id Target object UUID
 * @param label Optional connection label (can be NULL)
 * @param result Output parameter for connection ID
 * @param error Output parameter for error messages
 * @return 0 on success, -1 on error
 */
typedef int (*workspace_create_connection_fn)(const char* from_id, const char* to_id,
                                              const char* label, char** result, char** error);

/**
 * @brief Update or append content to an existing object
 *
 * @param object_id UUID of the object to update
 * @param content New content to add or replace
 * @param append String "true" or "false" to append or replace content
 * @param result Output parameter for JSON object with updated details
 * @param error Output parameter for error messages
 * @return 0 on success, -1 on error
 */
typedef int (*workspace_update_object_fn)(const char* object_id, const char* content,
                                          const char* append, char** result, char** error);

/**
 * @brief Export a markdown note to Word (.docx) format
 *
 * @param object_id UUID of the markdown note to export
 * @param result Output parameter for JSON object with export details (docx_path, etc.)
 * @param error Output parameter for error messages
 * @return 0 on success, -1 on error (e.g., pandoc not installed)
 */
typedef int (*workspace_export_to_docx_fn)(const char* object_id, char** result, char** error);

/**
 * @brief Highlight nodes in the graph visualization
 *
 * @param node_ids_json JSON array of node IDs to highlight (e.g., ["id1", "id2"])
 * @param result Output parameter for JSON response
 * @param error Output parameter for error messages
 * @return 0 on success, -1 on error
 */
typedef int (*workspace_highlight_nodes_fn)(const char* node_ids_json, char** result, char** error);

/**
 * @brief Collection of workspace operation function pointers
 *
 * This struct is populated by Rust and passed to the C plugin during
 * registration. It provides all the callbacks needed for workspace operations.
 */
typedef struct {
  workspace_list_objects_fn list_objects;
  workspace_search_objects_fn search_objects;
  workspace_get_object_fn get_object;
  workspace_create_note_fn create_note;
  workspace_update_object_fn update_object;
  workspace_create_connection_fn create_connection;
  workspace_export_to_docx_fn export_to_docx;
  workspace_highlight_nodes_fn highlight_nodes;
} workspace_operations_t;

#ifdef __cplusplus
}
#endif

#endif /* ETHERVOX_WORKSPACE_OPERATIONS_H */
