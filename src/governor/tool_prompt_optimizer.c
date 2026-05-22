/**
 * @file tool_prompt_optimizer.c
 * @brief Tool prompt optimizer with generation and runtime loading
 *
 * Generates model-specific optimized prompts in JSON format.
 * Provides runtime loading of optimizations for system prompt building.
 * Processes tools in batches to avoid KV cache overflow.
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ethervox/chat_template.h"
#include "ethervox/config.h"
#include "ethervox/error.h"
#include "ethervox/governor.h"
#include "ethervox/logging.h"
#include "ethervox/tool_manifest.h"
#include "ethervox/tool_prompt_optimizer.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

// Forward declaration for Android files directory getter
#ifdef ETHERVOX_PLATFORM_ANDROID
extern const char* ethervox_android_get_files_dir(void);
#endif

#define BATCH_SIZE 5  // Process 5 tools per batch to avoid KV overflow

// ANSI color codes
#define COLOR_RESET "\033[0m"
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"
#define COLOR_BOLD "\033[1m"

// Global cancellation flag
static volatile sig_atomic_t g_optimization_cancelled = 0;

// Test report file
static FILE* g_report_file = NULL;
static char g_report_path[512] = {0};

static void handle_sigint(int sig) {
  (void)sig;
  g_optimization_cancelled = 1;
  printf("\n\n⚠️  Optimization cancelled by user (Ctrl+C)\n");
  if (g_report_file) {
    fprintf(g_report_file, "\n\n⚠️  Optimization cancelled by user (Ctrl+C)\n");
    fflush(g_report_file);
  }
}

// Extract clean model name (e.g., "granite-4.0-Q4_K_M.gguf" -> "granite-4.0")
// Used by both generator and loader to ensure consistent naming
static void extract_model_family(const char* model_path, char* model_name, size_t max_len) {
  if (!model_path || !model_name || max_len == 0)
    return;

  // Find the last path separator (works for both Unix '/' and Windows '\')
  const char* filename = strrchr(model_path, '/');
  const char* filename_backslash = strrchr(model_path, '\\');

  // Use whichever separator was found last (rightmost in the path)
  if (filename_backslash && (!filename || filename_backslash > filename)) {
    filename = filename_backslash;
  }

  if (!filename)
    filename = model_path;
  else
    filename++;

  // Extract up to first dash followed by 'Q' (quantization marker)
  // or up to .gguf extension
  size_t i = 0;
  bool found_quant = false;

  while (i < max_len - 1 && filename[i]) {
    // Stop at .gguf
    if (filename[i] == '.' && strncmp(&filename[i], ".gguf", 5) == 0) {
      break;
    }

    // Stop at quantization marker (e.g., -Q4_K_M)
    if (filename[i] == '-' && i + 1 < strlen(filename) && filename[i + 1] == 'Q') {
      break;
    }

    // Stop at -h- (helper marker in some models)
    if (filename[i] == '-' && i + 2 < strlen(filename) && filename[i + 1] == 'h' &&
        filename[i + 2] == '-') {
      break;
    }

    model_name[i] = filename[i];
    i++;
  }
  model_name[i] = '\0';

  // Remove trailing dash if present
  if (i > 0 && model_name[i - 1] == '-') {
    model_name[i - 1] = '\0';
  }
}

// Count words in a string (for token estimation)
static int count_words(const char* text) {
  int count = 0;
  bool in_word = false;

  for (const char* p = text; *p; p++) {
    if (isspace(*p)) {
      in_word = false;
    } else if (!in_word) {
      in_word = true;
      count++;
    }
  }

  return count;
}

// Extract optimized sentence from LLM response
static void extract_optimized_sentence(const char* response, char* output, size_t max_len) {
  if (!response || !output || max_len == 0)
    return;

  // Look for "Call <tool> when..." pattern or "Use <tool> when..."
  const char* start = strstr(response, "Call ");
  if (!start)
    start = strstr(response, "Use ");
  if (!start) {
    // Fallback: take first sentence
    start = response;
    while (*start && isspace(*start))
      start++;
  }

  // Find end of sentence - must be period followed by space/newline/EOF
  // This avoids cutting off at periods inside parentheses like "(e.g., ...)"
  const char* end = start;
  int paren_depth = 0;
  int quote_depth = 0;

  while (*end) {
    if (*end == '(')
      paren_depth++;
    else if (*end == ')')
      paren_depth--;
    else if (*end == '"')
      quote_depth = !quote_depth;
    else if (*end == '.' && paren_depth == 0 && quote_depth == 0) {
      // Check if this is a sentence-ending period
      if (*(end + 1) == '\0' || *(end + 1) == '\n' || isspace(*(end + 1))) {
        end++;  // Include the period
        break;
      }
    } else if (*end == '\n' && paren_depth == 0 && quote_depth == 0) {
      break;
    }
    end++;
  }

  if (end == start)
    end = start + strlen(start);

  size_t len = end - start;
  if (len >= max_len)
    len = max_len - 1;

  strncpy(output, start, len);
  output[len] = '\0';

  // Trim trailing whitespace
  while (len > 0 && isspace(output[len - 1])) {
    output[--len] = '\0';
  }
}

// Ensure directory exists
static int ensure_directory(const char* path) {
#ifdef _WIN32
  return _mkdir(path);
#else
  return mkdir(path, 0755);
#endif
}

// Create ~/.ethervox/tools/optimized/ directory structure
// Parse existing JSON file to get list of already optimized tools
typedef struct {
  char name[64];
  char optimized_prompt[256];
  int token_count;
} optimized_tool_entry_t;

static int parse_existing_optimizations(const char* json_path, optimized_tool_entry_t** entries_out,
                                        uint32_t* count_out) {
  FILE* fp = fopen(json_path, "r");
  if (!fp) {
    *entries_out = NULL;
    *count_out = 0;
    return ETHERVOX_ERROR_INVALID_ARGUMENT;  // File doesn't exist yet
  }

  // Read entire file
  fseek(fp, 0, SEEK_END);
  long file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (file_size <= 0 || file_size > 1024 * 1024) {  // Max 1MB
    fclose(fp);
    *entries_out = NULL;
    *count_out = 0;
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  char* content = malloc(file_size + 1);
  if (!content) {
    fclose(fp);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  fread(content, 1, file_size, fp);
  content[file_size] = '\0';
  fclose(fp);

  // Count tools in JSON (count occurrences of '"name":')
  uint32_t tool_count = 0;
  const char* p = content;
  while ((p = strstr(p, "\"name\":")) != NULL) {
    tool_count++;
    p += 7;
  }

  if (tool_count == 0) {
    free(content);
    *entries_out = NULL;
    *count_out = 0;
    return ETHERVOX_SUCCESS;
  }

  // Allocate entries
  optimized_tool_entry_t* entries = calloc(tool_count, sizeof(optimized_tool_entry_t));
  if (!entries) {
    free(content);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Parse each tool entry
  uint32_t idx = 0;
  p = content;
  while ((p = strstr(p, "\"name\":")) != NULL && idx < tool_count) {
    p += 7;  // Skip past '"name":'
    while (*p && isspace(*p))
      p++;
    if (*p != '"')
      continue;
    p++;  // Skip opening quote

    // Extract name
    const char* name_end = strchr(p, '"');
    if (!name_end)
      continue;
    size_t name_len = name_end - p;
    if (name_len >= sizeof(entries[idx].name))
      name_len = sizeof(entries[idx].name) - 1;
    strncpy(entries[idx].name, p, name_len);
    entries[idx].name[name_len] = '\0';

    // Extract optimized_prompt (optional)
    const char* prompt_start = strstr(name_end, "\"optimized_prompt\":");
    if (prompt_start && prompt_start < name_end + 500) {  // Must be in same object
      prompt_start += 20;                                 // Skip past '"optimized_prompt":'
      while (*prompt_start && isspace(*prompt_start))
        prompt_start++;
      if (*prompt_start == '"') {
        prompt_start++;
        const char* prompt_end = strchr(prompt_start, '"');
        if (prompt_end) {
          size_t prompt_len = prompt_end - prompt_start;
          if (prompt_len >= sizeof(entries[idx].optimized_prompt)) {
            prompt_len = sizeof(entries[idx].optimized_prompt) - 1;
          }
          strncpy(entries[idx].optimized_prompt, prompt_start, prompt_len);
          entries[idx].optimized_prompt[prompt_len] = '\0';
        }
      }
    }

    // Extract token_count (optional)
    const char* token_start = strstr(name_end, "\"token_count\":");
    if (token_start && token_start < name_end + 500) {
      token_start += 15;  // Skip past '"token_count":'
      while (*token_start && isspace(*token_start))
        token_start++;
      entries[idx].token_count = atoi(token_start);
    }

    idx++;
    p = name_end + 1;
  }

  free(content);
  *entries_out = entries;
  *count_out = idx;
  return ETHERVOX_SUCCESS;
}

// Check if a tool is already optimized
static bool is_tool_optimized(const char* tool_name, const optimized_tool_entry_t* entries,
                              uint32_t entry_count) {
  for (uint32_t i = 0; i < entry_count; i++) {
    if (strcmp(tool_name, entries[i].name) == 0) {
      return true;
    }
  }
  return false;
}

// Create ~/.ethervox/tools/optimized/ directory structure
static int create_optimized_dir(void) {
#ifdef ETHERVOX_PLATFORM_ANDROID
  // On Android, use app files directory
  const char* android_files_dir = ethervox_android_get_files_dir();
  if (!android_files_dir || android_files_dir[0] == '\0') {
    ETHERVOX_LOGE("Android files directory not set");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  char dir1[512], dir2[512];
  snprintf(dir1, sizeof(dir1), "%s/tools", android_files_dir);
  snprintf(dir2, sizeof(dir2), "%s/tools/optimized", android_files_dir);

  ensure_directory(dir1);
  ensure_directory(dir2);
#elif defined(_WIN32)
  // On Windows, use USERPROFILE and backslashes
  const char* home = getenv("USERPROFILE");
  if (!home)
    home = getenv("HOME");  // Fallback
  if (!home) {
    ETHERVOX_LOGE("USERPROFILE environment variable not set");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  char dir1[512], dir2[512], dir3[512];
  snprintf(dir1, sizeof(dir1), "%s\\.ethervox", home);
  snprintf(dir2, sizeof(dir2), "%s\\.ethervox\\tools", home);
  snprintf(dir3, sizeof(dir3), "%s\\.ethervox\\tools\\optimized", home);

  ensure_directory(dir1);
  ensure_directory(dir2);
  ensure_directory(dir3);
#else
  // On Unix/Linux/macOS, use HOME and forward slashes
  const char* home = getenv("HOME");
  if (!home) {
    ETHERVOX_LOGE("HOME environment variable not set");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  char dir1[512], dir2[512], dir3[512];
  snprintf(dir1, sizeof(dir1), "%s/.ethervox", home);
  snprintf(dir2, sizeof(dir2), "%s/.ethervox/tools", home);
  snprintf(dir3, sizeof(dir3), "%s/.ethervox/tools/optimized", home);

  ensure_directory(dir1);
  ensure_directory(dir2);
  ensure_directory(dir3);
#endif

  return ETHERVOX_SUCCESS;
}

/**
 * Generate filepath for model-specific prompts in Android files directory or ~/.ethervox/
 */
static void get_prompt_file_path(const char* model_path, char* output, size_t max_len) {
  char family[128];
  extract_model_family(model_path, family, sizeof(family));
  
#ifdef ETHERVOX_PLATFORM_ANDROID
  // Android: Try files directory (set via setAndroidFilesDir JNI call)
  const char* android_files = ethervox_android_get_files_dir();
  if (android_files && android_files[0] != '\0') {
    snprintf(output, max_len, "%s/tools/optimized/%s.json", android_files, family);
    return;
  }
#endif
  
  // Fallback to HOME directory for desktop/Mac/Linux
  const char* home = getenv("HOME");
  if (home) {
    snprintf(output, max_len, "%s/.ethervox/tools/optimized/%s.json", home, family);
  } else {
    snprintf(output, max_len, "./tools/optimized/%s.json", family);
  }
}

/**
 * Optimize tool prompts and write to JSON cache (generator)
 */
ethervox_result_t ethervox_optimize_tool_prompts(ethervox_governor_t* governor,
                                                 const char* model_path,
                                                 tool_manifest_registry_t* manifest_registry,
                                                 bool optimize_new_only) {
  if (!governor || !model_path || !manifest_registry) {
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Check tool count instead of tools_available flag
  // (tools_available may be false when optimization file doesn't exist yet)
  if (manifest_registry->header.tool_count == 0) {
    ETHERVOX_LOGE("No tools in manifest (count: 0)");
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Set up signal handler for Ctrl+C
  g_optimization_cancelled = 0;
#ifndef _WIN32
  struct sigaction sa;
  sa.sa_handler = handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
#else
  signal(SIGINT, handle_sigint);
#endif

  // Create test report file
  char ethervox_dir[512];
  if (ethervox_is_success(
          ethervox_get_runtime_path("reports", ethervox_dir, sizeof(ethervox_dir)))) {
    // Ensure reports directory exists
#ifdef _WIN32
    _mkdir(ethervox_dir);
#else
    mkdir(ethervox_dir, 0755);
#endif

    time_t now = time(NULL);
    snprintf(g_report_path, sizeof(g_report_path), "%s/tool_optimization_%ld.txt", ethervox_dir,
             (long)now);
    g_report_file = fopen(g_report_path, "w");
    if (g_report_file) {
      printf("Report: %s\n", g_report_path);
      fprintf(g_report_file, "═══════════════════════════════════════════════════════════════\n");
      fprintf(g_report_file, " Tool Prompt Optimization Report\n");
      fprintf(g_report_file, "═══════════════════════════════════════════════════════════════\n\n");
      fprintf(g_report_file, "Timestamp: %s", ctime(&now));
      fprintf(g_report_file, "Model: %s\n\n", model_path);
      fflush(g_report_file);
    } else {
      fprintf(stderr, "Warning: Failed to create report file at %s\n", g_report_path);
    }
  } else {
    fprintf(stderr, "Warning: Failed to get reports directory path\n");
  }

  // Detect chat template from model path to get tool call format
  chat_template_type_t template_type = chat_template_detect(model_path);
  const chat_template_t* template = chat_template_get(template_type, model_path);
  if (!template) {
    ETHERVOX_LOGW("Could not detect chat template, using default XML format");
  }

  // Extract tool call format example from template's usage examples
  // This ensures we're model-agnostic
  char tool_call_example[256] = "<tool_call name=\"TOOL_NAME\" param=\"value\" />";
  if (template && template->tool_usage_examples) {
    // Try to extract the actual format used in examples
    const char* example_start = strstr(template->tool_usage_examples, "<tool_call");
    if (example_start) {
      const char* example_end = strstr(example_start, "/>");
      if (example_end) {
        size_t len = (example_end - example_start) + 2;
        if (len < sizeof(tool_call_example)) {
          strncpy(tool_call_example, example_start, len);
          tool_call_example[len] = '\0';
        }
      }
    }
  }

  // Extract model name
  char model_name[128];
  extract_model_family(model_path, model_name, sizeof(model_name));

  ETHERVOX_LOGI("Starting tool prompt optimization for: %s", model_name);

  // Create output directory
  if (create_optimized_dir() != 0) {
    ETHERVOX_LOGW("Failed to create optimized directory");
  }

  // Build output path
  char output_path[512];
#ifdef ETHERVOX_PLATFORM_ANDROID
  const char* android_files_dir = ethervox_android_get_files_dir();
  snprintf(output_path, sizeof(output_path), "%s/tools/optimized/%s.json", android_files_dir,
           model_name);
#elif defined(_WIN32)
  // On Windows, use USERPROFILE and backslashes
  const char* home = getenv("USERPROFILE");
  if (!home)
    home = getenv("HOME");  // Fallback
  if (!home)
    home = ".";  // Last resort
  snprintf(output_path, sizeof(output_path), "%s\\.ethervox\\tools\\optimized\\%s.json", home,
           model_name);
#else
  // On Unix/Linux/macOS, use HOME and forward slashes
  const char* home = getenv("HOME");
  snprintf(output_path, sizeof(output_path), "%s/.ethervox/tools/optimized/%s.json", home,
           model_name);
#endif

  // Parse existing optimizations if in incremental mode
  optimized_tool_entry_t* existing_entries = NULL;
  uint32_t existing_count = 0;
  uint32_t tools_to_optimize = manifest_registry->header.tool_count;
  uint32_t tools_skipped = 0;

  if (optimize_new_only) {
    int parse_result =
        parse_existing_optimizations(output_path, &existing_entries, &existing_count);
    if (parse_result == 0 && existing_count > 0) {
      printf("\nIncremental mode: Found %u existing optimizations\n", existing_count);
      if (g_report_file) {
        fprintf(g_report_file, "\nIncremental mode: Found %u existing optimizations\n",
                existing_count);
        fprintf(g_report_file, "Will only optimize new tools\n\n");
        fflush(g_report_file);
      }

      // Count tools that need optimization
      tools_to_optimize = 0;
      for (uint32_t i = 0; i < manifest_registry->header.tool_count; i++) {
        const tool_index_entry_t* tool_idx = &manifest_registry->index[i];
        if (tool_idx->enabled &&
            !is_tool_optimized(tool_idx->name, existing_entries, existing_count)) {
          tools_to_optimize++;
        }
      }
      tools_skipped = manifest_registry->header.tool_count - tools_to_optimize;
    } else {
      printf("\nIncremental mode: No existing optimizations found, will optimize all tools\n");
    }
  }

  printf("\n");
  printf("Starting optimization...\n");
  printf("Model: %s\n", model_name);
  printf("Total tools: %u\n", manifest_registry->header.tool_count);
  if (optimize_new_only && existing_count > 0) {
    printf("Already optimized: %u\n", tools_skipped);
    printf("To optimize: %u\n", tools_to_optimize);
  } else {
    printf("To optimize: %u\n", tools_to_optimize);
  }
  printf("Output: %s\n", output_path);
  printf("Batch size: %d tools/batch\n", BATCH_SIZE);
  printf("\n");
  printf("This will generate model-specific optimized prompts (~15 tokens/tool).\n");
  if (tools_to_optimize > 0) {
    printf("Estimated time: ~%d seconds\n", (tools_to_optimize / BATCH_SIZE + 1) * 10);
  }
  printf("\n");

  // DEBUG: Check manifest state
  ETHERVOX_LOGI("Manifest state before optimization:");
  ETHERVOX_LOGI("  manifest_file=%p", (void*)manifest_registry->manifest_file);
  ETHERVOX_LOGI("  header.tool_count=%u", manifest_registry->header.tool_count);
  ETHERVOX_LOGI("  tools_available=%d", manifest_registry->tools_available);
  ETHERVOX_LOGI("  fallback_level=%u", manifest_registry->fallback_level);

  // CRITICAL FIX: During optimization, we need access to the manifest even if
  // tools_available is false (which happens when optimized file doesn't exist yet).
  // Temporarily set tools_available = true so ethervox_tool_get_index() doesn't fail.
  bool original_tools_available = manifest_registry->tools_available;
  manifest_registry->tools_available = true;

  // If no new tools to optimize, we're done
  if (tools_to_optimize == 0) {
    printf(COLOR_GREEN "[OK]" COLOR_RESET " All tools already optimized!\n");
    manifest_registry->tools_available = original_tools_available;  // Restore state
    if (existing_entries)
      free(existing_entries);
    if (g_report_file) {
      fprintf(g_report_file, "\nAll tools already optimized - nothing to do\n");
      fclose(g_report_file);
    }
    return ETHERVOX_SUCCESS;
  }

  // Open output file
  FILE* fp = fopen(output_path, "w");
  if (!fp) {
    ETHERVOX_LOGE("Failed to open output file: %s", output_path);
    if (existing_entries)
      free(existing_entries);
    return ETHERVOX_ERROR_INVALID_ARGUMENT;
  }

  // Write JSON header
  fprintf(fp, "{\n");
  fprintf(fp, "  \"model_name\": \"%s\",\n", model_name);
  fprintf(fp, "  \"optimized_at\": %ld,\n", (long)time(NULL));
  fprintf(fp, "  \"tools\": [\n");

  // First, write existing optimized tools (if in incremental mode)
  bool first_entry = true;
  if (optimize_new_only && existing_entries && existing_count > 0) {
    for (uint32_t i = 0; i < existing_count; i++) {
      if (!first_entry)
        fprintf(fp, ",\n");
      fprintf(fp, "    {\n");
      fprintf(fp, "      \"name\": \"%s\",\n", existing_entries[i].name);
      fprintf(fp, "      \"optimized_prompt\": \"%s\",\n", existing_entries[i].optimized_prompt);
      fprintf(fp, "      \"token_count\": %d\n", existing_entries[i].token_count);
      fprintf(fp, "    }");
      first_entry = false;
    }
  }

  uint32_t total_tools = manifest_registry->header.tool_count;
  uint32_t tools_processed = 0;

  // Process tools in batches
  for (uint32_t batch_start = 0; batch_start < total_tools; batch_start += BATCH_SIZE) {
    if (g_optimization_cancelled) {
      printf("\n⚠️  Optimization cancelled - cleaning up...\n");
      if (g_report_file) {
        fprintf(g_report_file, "\n⚠️  Optimization cancelled before completion\n");
        fprintf(g_report_file, "Tools processed: %u/%u\n", tools_processed, total_tools);
        fflush(g_report_file);
        fclose(g_report_file);
        printf("📄 Partial report saved: %s\n", g_report_path);
      }
      fclose(fp);
      return ETHERVOX_ERROR_INTERRUPTED;
    }

    uint32_t batch_end = batch_start + BATCH_SIZE;
    if (batch_end > total_tools)
      batch_end = total_tools;

    printf("\n[Batch %u/%u] Processing tools %u-%u... (Ctrl+C to cancel)\n",
           (batch_start / BATCH_SIZE) + 1, (total_tools + BATCH_SIZE - 1) / BATCH_SIZE,
           batch_start + 1, batch_end);

    if (g_report_file) {
      fprintf(g_report_file, "\n[Batch %u/%u] Processing tools %u-%u\n",
              (batch_start / BATCH_SIZE) + 1, (total_tools + BATCH_SIZE - 1) / BATCH_SIZE,
              batch_start + 1, batch_end);
      fflush(g_report_file);
    }

    // Reset conversation to clear KV cache between batches
    ethervox_governor_reset_conversation(governor);

    for (uint32_t i = batch_start; i < batch_end; i++) {
      if (g_optimization_cancelled) {
        printf("\n⚠️  Optimization cancelled\n");
        if (g_report_file) {
          fprintf(g_report_file, "\n⚠️  Optimization cancelled\n");
          fprintf(g_report_file, "Tools processed: %u/%u\n", tools_processed, total_tools);
          fflush(g_report_file);
          fclose(g_report_file);
        }
        fclose(fp);
        return ETHERVOX_ERROR_INTERRUPTED;
      }
      const tool_index_entry_t* tool_idx = &manifest_registry->index[i];

      if (!tool_idx->enabled) {
        printf("  ⊘ %s: disabled, skipping\n", tool_idx->name);
        continue;
      }

      // Skip if already optimized (in incremental mode)
      if (optimize_new_only && existing_entries &&
          is_tool_optimized(tool_idx->name, existing_entries, existing_count)) {
        printf("  " COLOR_CYAN "↻" COLOR_RESET " %s: already optimized, skipping\n",
               tool_idx->name);
        if (g_report_file) {
          fprintf(g_report_file, "  ↻ %s: already optimized, skipping\n", tool_idx->name);
          fflush(g_report_file);
        }
        continue;
      }

      // Get full tool detail
      tool_detail_header_t detail;
      // Allocate params on heap to avoid stack overflow (7.7KB array)
      tool_param_t* params = malloc(MAX_PARAMETERS * sizeof(tool_param_t));
      if (!params) {
        printf("  [FAIL] %s: memory allocation failed for params\n", tool_idx->name);
        continue;
      }
      uint8_t param_count;

      if (ethervox_tool_get_detail(manifest_registry, tool_idx->name, &detail, params,
                                   &param_count) != 0) {
        printf("  [FAIL] %s: failed to load detail\n", tool_idx->name);
        free(params);
        continue;
      }

      // Build optimization query - model-agnostic
      // Allocate on heap to avoid stack overflow
      char* query = malloc(4096);
      if (!query) {
        printf("  [FAIL] %s: memory allocation failed\n", tool_idx->name);
        free(params);
        continue;
      }
      int qoff =
          snprintf(query, 4096,
                   "Tool: %s\n"
                   "Category: %s\n"
                   "Description: %s\n"
                   "Parameters (%u):\n",
                   tool_idx->name, tool_idx->category, detail.description, detail.param_count);

      // Add parameter list
      for (uint8_t p = 0; p < param_count && qoff < 4096 - 500; p++) {
        qoff += snprintf(query + qoff, 4096 - qoff, "  - %s (%s, %s)\n", params[p].name,
                         params[p].type, params[p].required ? "required" : "optional");
      }

      // Build tool call format example using actual parameter names
      // Detect tool format from chat template
      const chat_template_t* chat_template = ethervox_governor_get_chat_template(governor);
      tool_format_type_t tool_fmt =
          chat_template ? chat_template_get_tool_format(chat_template) : TOOL_FORMAT_XML_ATTR;

      // Allocate on heap to avoid stack overflow
      char* tool_format = malloc(512);
      if (!tool_format) {
        printf("  [FAIL] %s: memory allocation failed for tool_format\n", tool_idx->name);
        free(query);
        free(params);
        continue;
      }

      if (tool_fmt == TOOL_FORMAT_JSON_IN_XML) {
        // Granite 4.0 format: <tool_call>{"name":"...","arguments":{...}}</tool_call>
        int foff = snprintf(tool_format, 512, "<tool_call>\n{\"name\": \"%s\", \"arguments\": {",
                            tool_idx->name);
        for (uint8_t p = 0; p < param_count && foff < 512 - 100; p++) {
          if (p > 0) {
            foff += snprintf(tool_format + foff, 512 - foff, ", ");
          }
          foff += snprintf(tool_format + foff, 512 - foff, "\"%s\": \"value\"", params[p].name);
        }
        snprintf(tool_format + foff, 512 - foff, "}}\n</tool_call>");
      } else {
        // XML attribute format (Qwen, Llama, Phi)
        int foff = snprintf(tool_format, 512, "<tool_call name=\"%s\"", tool_idx->name);
        for (uint8_t p = 0; p < param_count && foff < 512 - 50; p++) {
          foff += snprintf(tool_format + foff, 512 - foff, " %s=\"value\"", params[p].name);
        }
        snprintf(tool_format + foff, 512 - foff, " />");
      }

      // Add special guidance for memory tools
      const char* memory_guidance = "";
      if (strcmp(tool_idx->name, "memory_store") == 0) {
        memory_guidance =
            "\n\nCRITICAL: This tool stores information to PERSISTENT LONG-TERM MEMORY. "
            "Call it when user says 'remember', shares preferences, facts about themselves, "
            "or provides corrections. Memory survives conversation resets and app restarts.";
      } else if (strcmp(tool_idx->name, "memory_search") == 0) {
        memory_guidance =
            "\n\nCRITICAL: This tool searches PERSISTENT LONG-TERM MEMORY. "
            "Call it when asked to recall previously stored information (e.g., 'what is my...', "
            "'do you remember...', 'what did I say about...'). "
            "Do NOT answer from conversation context alone - always check persistent storage.";
      }

      qoff += snprintf(
          query + qoff, 4096 - qoff,
          "\nYou must generate a brief natural language description (under 30 words) explaining:\n"
          "- WHEN to use this tool (what user requests trigger it)\n"
          "- WHAT it does\n%s\n\n"
          "Do NOT output XML tags or tool calls. Output plain English text only.\n"
          "Example format: \"Use %s when the user asks to [scenario]. It [what it does].\"\n"
          "Your description:",
          memory_guidance, tool_idx->name);

      // Hardcode critical memory tool prompts (LLM-generated ones are too weak)
      const char* hardcoded_prompt = NULL;
      int hardcoded_tokens = 0;

      if (strcmp(tool_idx->name, "memory_store") == 0) {
        hardcoded_prompt =
            "ALWAYS call when user says 'remember' or shares personal facts/preferences. Stores to "
            "persistent long-term memory.";
        hardcoded_tokens = 15;
      } else if (strcmp(tool_idx->name, "memory_search") == 0) {
        hardcoded_prompt =
            "ALWAYS call when asked 'what is my...', 'do you remember...'. Searches persistent "
            "memory, not conversation context.";
        hardcoded_tokens = 17;
      }

      if (hardcoded_prompt) {
        // Use hardcoded prompt for critical memory tools
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", tool_idx->name);
        fprintf(fp, "      \"optimized_prompt\": \"%s\",\n", hardcoded_prompt);
        fprintf(fp, "      \"token_count\": %d\n", hardcoded_tokens);
        fprintf(fp, "    }%s\n", (i < total_tools - 1) ? "," : "");

        printf("  " COLOR_CYAN "⚙" COLOR_RESET " %s: %s (%d tokens) " COLOR_YELLOW
               "[hardcoded]" COLOR_RESET "\n",
               tool_idx->name, hardcoded_prompt, hardcoded_tokens);

        if (g_report_file) {
          fprintf(g_report_file, "  ⚙ %s [hardcoded]\n", tool_idx->name);
          fprintf(g_report_file, "    Optimized: %s\n", hardcoded_prompt);
          fprintf(g_report_file, "    Tokens: %d\n", hardcoded_tokens);
          fflush(g_report_file);
        }

        tools_processed++;
        free(query);
        free(tool_format);
        free(params);
        continue;  // Skip LLM generation for this tool
      }

      // Disable tool execution so the LLM's example tool call isn't executed
      ethervox_governor_set_tool_execution(governor, false);

      // Ask LLM
      char* response = NULL;
      char* error = NULL;

      ETHERVOX_LOGI("Sending query to LLM for tool '%s' (query length: %d)", tool_idx->name, qoff);

      if (ethervox_governor_execute(governor, query, &response, &error, NULL, NULL, NULL, NULL) ==
              0 &&
          response) {
        // Re-enable tool execution
        ethervox_governor_set_tool_execution(governor, true);

        ETHERVOX_LOGI("LLM response for '%s': '%.200s%s'", tool_idx->name, response,
                      strlen(response) > 200 ? "..." : "");

        // Extract optimized sentence
        char optimized[256];
        extract_optimized_sentence(response, optimized, sizeof(optimized));

        ETHERVOX_LOGI("Extracted optimized prompt: '%s'", optimized);

        // Estimate tokens (~0.75 tokens per word)
        int word_count = count_words(optimized);
        int token_estimate = (int)(word_count * 0.75);

        // Write to JSON (always add comma - we're in incremental mode after existing entries)
        if (!first_entry)
          fprintf(fp, ",\n");
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", tool_idx->name);
        fprintf(fp, "      \"optimized_prompt\": \"%s\",\n", optimized);
        fprintf(fp, "      \"token_count\": %d\n", token_estimate);
        fprintf(fp, "    }");
        first_entry = false;

        printf("  " COLOR_GREEN "[OK]" COLOR_RESET " %s: %s (%d tokens)\n", tool_idx->name,
               optimized, token_estimate);

        if (g_report_file) {
          fprintf(g_report_file, "  [OK] %s\n", tool_idx->name);
          fprintf(g_report_file, "    Optimized: %s\n", optimized);
          fprintf(g_report_file, "    Tokens: %d\n", token_estimate);
          fflush(g_report_file);
        }

        tools_processed++;
      } else {
        // Re-enable tool execution even on failure
        ethervox_governor_set_tool_execution(governor, true);

        printf("  [FAIL] %s: optimization failed - %s\n", tool_idx->name,
               error ? error : "unknown");

        if (g_report_file) {
          fprintf(g_report_file, "  [FAIL] %s: optimization failed\n", tool_idx->name);
          if (error) {
            fprintf(g_report_file, "    Error: %s\n", error);
          }
          fflush(g_report_file);
        }

        // Write fallback entry using one-liner
        if (!first_entry)
          fprintf(fp, ",\n");
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"name\": \"%s\",\n", tool_idx->name);
        fprintf(fp, "      \"optimized_prompt\": \"%s\",\n", tool_idx->one_line);
        fprintf(fp, "      \"token_count\": %d\n", count_words(tool_idx->one_line));
        fprintf(fp, "    }");
        first_entry = false;
      }

      free(response);
      free(error);
      free(tool_format);  // Free heap-allocated tool_format buffer
      free(query);        // Free heap-allocated query buffer
      free(params);       // Free heap-allocated params array
    }
  }

  // Close JSON
  fprintf(fp, "  ]\n");
  fprintf(fp, "}\n");
  fclose(fp);

  printf("\n" COLOR_GREEN "[OK]" COLOR_RESET " Optimization complete!\n");
  printf("  Tools processed: %u/%u\n", tools_processed, total_tools);
  printf("  Output file: %s\n", output_path);

  // Write final statistics to report
  if (g_report_file) {
    fprintf(g_report_file, "\n═══════════════════════════════════════════════════════════════\n");
    fprintf(g_report_file, " Optimization Summary\n");
    fprintf(g_report_file, "═══════════════════════════════════════════════════════════════\n\n");
    fprintf(g_report_file, "Total tools: %u\n", total_tools);
    fprintf(g_report_file, "Successfully optimized: %u\n", tools_processed);
    fprintf(g_report_file, "Failed: %u\n", total_tools - tools_processed);
    fprintf(g_report_file, "Success rate: %.1f%%\n\n", (tools_processed * 100.0) / total_tools);
    fprintf(g_report_file, "Output file: %s\n", output_path);
    fprintf(g_report_file, "\n═══════════════════════════════════════════════════════════════\n");
    fprintf(g_report_file, " Optimization Complete\n");
    fprintf(g_report_file, "═══════════════════════════════════════════════════════════════\n");
    fflush(g_report_file);
    fclose(g_report_file);
    printf("\n" COLOR_CYAN "📄 Report saved: %s" COLOR_RESET "\n", g_report_path);
  }

  printf("\nRestart EthervoxAI to use optimized prompts.\n");

  // Restore original tools_available state
  manifest_registry->tools_available = original_tools_available;

  // Cleanup
  if (existing_entries) {
    free(existing_entries);
  }

  return ETHERVOX_SUCCESS;
}
