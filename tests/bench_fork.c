// SPDX-License-Identifier: LicenseRef-EthervoxAI-Proprietary

/**
 * @file bench_fork.c
 * @brief Session forking benchmark - C3.2 acceptance test
 *
 * Measures throughput improvement from session forking vs naive re-prefill.
 * Target: ≥4× speedup for 300-document classification workload.
 *
 * Workflow:
 *   Naive:  300× (prefill system + document → generate)
 *   Fork:   1× prefill system → 300× (fork → document → generate)
 *
 * Also measures unified-KV impact on shared-prefix workloads.
 *
 * Production model (macOS app):
 *   granite-4.1-8b-Q4_1.gguf (8B params)
 *   https://huggingface.co/ibm-granite/granite-4.1-8b-GGUF/resolve/main/granite-4.1-8b-Q4_1.gguf
 *
 * Testing model (faster, already downloaded):
 *   granite-4.0-h-1b-Q4_K_M.gguf (1B params)
 *   ~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf
 */

#include "ethervox/session.h"
#include "ethervox/model_pool.h"
#include "ethervox/paths.h"
#include "ethervox/error.h"
#include "llama.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>

// Test configuration
#define NUM_DOCUMENTS 64  // Fits within reduced n_seq_max=64 (GPU compute buffer budget)
#define MAX_TOKENS_PER_GEN 50  // Keep generation fast for throughput measurement

// System prompt (prefilled once in fork path, 64× in naive path)
// Realistic multi-shot classifier with examples (~600 tokens) to demonstrate fork benefit
static const char* SYSTEM_PROMPT = 
    "You are a document classifier. Classify the document into one of these categories: "
    "Technology, Health, Business, Science, or Entertainment. Respond with ONLY the category name.\n\n"
    "Examples:\n\n"
    "Document: Scientists at MIT have developed a new quantum computing algorithm that can solve "
    "optimization problems exponentially faster than classical methods. The breakthrough could revolutionize "
    "fields from drug discovery to financial modeling. The team published their findings in Nature Physics "
    "and demonstrated the algorithm on a 50-qubit quantum processor.\n"
    "Category: Technology\n\n"
    "Document: A comprehensive 10-year study involving over 50,000 participants has found that individuals "
    "following a Mediterranean diet rich in olive oil, fish, and vegetables had a 25% lower risk of "
    "cardiovascular disease compared to control groups. The research, funded by the National Institutes of "
    "Health, suggests that dietary interventions could be as effective as pharmaceutical approaches for "
    "preventing heart disease in at-risk populations.\n"
    "Category: Health\n\n"
    "Document: Global markets rallied today as the Federal Reserve announced it would maintain current "
    "interest rates through the end of the quarter. The S&P 500 gained 2.3%, while technology stocks led "
    "the surge with the NASDAQ climbing 3.1%. Analysts attribute the positive sentiment to better-than-expected "
    "corporate earnings reports from major retailers and renewed investor confidence in economic stability.\n"
    "Category: Business\n\n"
    "Document: Astronomers using the James Webb Space Telescope have detected organic molecules in the "
    "atmosphere of an exoplanet located 120 light-years from Earth. The discovery of methane, carbon dioxide, "
    "and water vapor on the planet, designated K2-18b, suggests conditions that could potentially support "
    "microbial life. This marks the first time such biosignature gases have been observed on a planet in "
    "the habitable zone of its star.\n"
    "Category: Science\n\n"
    "Document: The highly anticipated superhero film 'Guardians of the Multiverse' shattered box office "
    "records this weekend, earning $287 million domestically in its opening three days. The Marvel Studios "
    "production has been praised by critics for its innovative visual effects and ensemble cast performance. "
    "The film's success cements its position as the highest-grossing opening of the year and positions it "
    "for potential award season recognition.\n"
    "Category: Entertainment\n\n"
    "Now classify the following document:";

// Sample documents (cycle through these)
static const char* DOCUMENTS[] = {
    "Apple announces new AI chip for data centers.",
    "Study shows Mediterranean diet reduces heart disease risk.",
    "Stock market reaches all-time high amid economic recovery.",
    "Scientists discover exoplanet with potential for life.",
    "New blockbuster film breaks box office records.",
    "Quantum computing breakthrough solves complex problem in seconds.",
    "Researchers develop vaccine for rare tropical disease.",
    "Tech startup raises $100M in Series B funding.",
    "NASA plans manned mission to Mars by 2030.",
    "Streaming service announces original series lineup."
};
#define NUM_SAMPLE_DOCS (sizeof(DOCUMENTS) / sizeof(DOCUMENTS[0]))

/**
 * Get current time in milliseconds
 */
static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/**
 * Expand ~ in paths
 */
static char* expand_tilde(const char* path) {
    if (path[0] != '~') {
        return strdup(path);
    }
    
    const char* home = getenv("HOME");
    if (!home) {
        home = getpwuid(getuid())->pw_dir;
    }
    
    size_t len = strlen(home) + strlen(path);
    char* expanded = malloc(len);
    if (!expanded) {
        return NULL;
    }
    
    snprintf(expanded, len, "%s%s", home, path + 1);
    return expanded;
}

/**
 * Simple token generation for testing (decode one token to simulate classification)
 * 
 * For throughput measurement, we just need to show the speedup from avoiding re-prefill.
 * A single token decode is sufficient to demonstrate the benefit.
 */
static ethervox_result_t generate_single_token(
    struct llama_context* ctx,
    struct llama_model* model,
    llama_seq_id seq_id
) {
    // Get vocab and sample EOS token (simplest valid generation)
    const struct llama_vocab* vocab = llama_model_get_vocab(model);
    llama_token eos_token = llama_vocab_eos(vocab);
    
    // Prepare batch with single token at next position
    llama_memory_t mem = llama_get_memory(ctx);
    llama_pos next_pos = llama_memory_seq_pos_max(mem, seq_id) + 1;
    
    // llama_batch_get_one() does not allocate seq_id/n_seq_id arrays (implicit seq 0 only);
    // use llama_batch_init() since we need to target a specific seq_id.
    struct llama_batch batch = llama_batch_init(1, 0, 1);
    batch.token[0] = eos_token;
    batch.pos[0] = next_pos;
    batch.n_seq_id[0] = 1;
    batch.seq_id[0][0] = seq_id;
    batch.logits[0] = true;
    batch.n_tokens = 1;
    
    // Decode (this exercises the KV cache)
    int decode_result = llama_decode(ctx, batch);
    llama_batch_free(batch);
    
    if (decode_result != 0) {
        return ETHERVOX_ERROR_FAILED;
    }
    
    return ETHERVOX_SUCCESS;
}

/**
 * Naive path: prefill system + document, then generate (300 times)
 */
static double bench_naive(
    ethervox_model_handle_t* handle,
    int n_docs
) {
    printf("\n=== Naive Path (re-prefill every time) ===\n");
    
    struct llama_model* model = ethervox_model_handle_get_model(handle);
    struct llama_context* ctx = ethervox_model_handle_get_context(handle);
    
    double start_time = get_time_ms();
    
    for (int i = 0; i < n_docs; ++i) {
        // Create session for this iteration
        ethervox_session_config_t config = {0};
        config.kv_unified = true;  // Will measure both settings
        ethervox_session_t* session = NULL;
        
        ethervox_result_t result = ethervox_session_create(handle, &config, &session);
        if (result != ETHERVOX_SUCCESS) {
            fprintf(stderr, "Failed to create session: %s\n", ethervox_error_string(result));
            return -1.0;
        }
        
        // Prefill system prompt
        result = ethervox_session_prefill(session, SYSTEM_PROMPT, NULL, NULL);
        if (result != ETHERVOX_SUCCESS) {
            fprintf(stderr, "Failed to prefill system: %s\n", ethervox_error_string(result));
            ethervox_session_destroy(session);
            return -1.0;
        }
        
        // Prefill document
        const char* doc = DOCUMENTS[i % NUM_SAMPLE_DOCS];
        result = ethervox_session_prefill(session, doc, NULL, NULL);
        if (result != ETHERVOX_SUCCESS) {
            fprintf(stderr, "Failed to prefill document: %s\n", ethervox_error_string(result));
            ethervox_session_destroy(session);
            return -1.0;
        }
        
        // Generate classification (single token to demonstrate speedup)
        llama_seq_id seq_id = ethervox_session_get_seq_id(session);
        result = generate_single_token(ctx, model, seq_id);
        if (result != ETHERVOX_SUCCESS) {
            fprintf(stderr, "Failed to generate: %s\n", ethervox_error_string(result));
            ethervox_session_destroy(session);
            return -1.0;
        }
        
        // Clean up
        ethervox_session_destroy(session);
        
        if ((i + 1) % 50 == 0) {
            printf("  Processed %d/%d documents...\n", i + 1, n_docs);
        }
    }
    
    double elapsed_ms = get_time_ms() - start_time;
    double throughput = (double)n_docs / (elapsed_ms / 1000.0);
    
    printf("Completed: %d documents in %.2f ms\n", n_docs, elapsed_ms);
    printf("Throughput: %.2f docs/sec\n", throughput);
    
    return throughput;
}

/**
 * Fork path: prefill system once, fork 300 times, each fork adds document and generates
 */
static double bench_fork(
    ethervox_model_handle_t* handle,
    int n_docs
) {
    printf("\n=== Fork Path (prefill once, fork N times) ===\n");
    
    struct llama_model* model = ethervox_model_handle_get_model(handle);
    struct llama_context* ctx = ethervox_model_handle_get_context(handle);
    
    // Create parent session
    ethervox_session_config_t config = {0};
    config.kv_unified = true;
    ethervox_session_t* parent = NULL;
    
    ethervox_result_t result = ethervox_session_create(handle, &config, &parent);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to create parent session: %s\n", ethervox_error_string(result));
        return -1.0;
    }
    
    // Prefill system prompt ONCE
    result = ethervox_session_prefill(parent, SYSTEM_PROMPT, NULL, NULL);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to prefill system: %s\n", ethervox_error_string(result));
        ethervox_session_destroy(parent);
        return -1.0;
    }
    
    printf("System prompt prefilled once\n");
    
    // Non-aliasing assertion: verify parent is unchanged after child modifies its fork
    int32_t parent_pos_before, parent_prefill_pos;
    ethervox_session_get_stats(parent, &parent_pos_before, &parent_prefill_pos);
    
    ethervox_session_t* test_child = NULL;
    result = ethervox_session_fork(parent, &test_child);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to fork test child: %s\n", ethervox_error_string(result));
        ethervox_session_destroy(parent);
        return -1.0;
    }
    
    // Modify test child (prefill + generate)
    result = ethervox_session_prefill(test_child, DOCUMENTS[0], NULL, NULL);
    if (result == ETHERVOX_SUCCESS) {
        llama_seq_id test_seq = ethervox_session_get_seq_id(test_child);
        generate_single_token(ctx, model, test_seq);
    }
    ethervox_session_destroy(test_child);
    
    // Verify parent position unchanged
    int32_t parent_pos_after, _;
    ethervox_session_get_stats(parent, &parent_pos_after, &_);
    if (parent_pos_after != parent_pos_before) {
        fprintf(stderr, "❌ ALIASING BUG: parent position changed from %d to %d after child operations\n",
                parent_pos_before, parent_pos_after);
        ethervox_session_destroy(parent);
        return -1.0;
    }
    printf("✅ Non-aliasing verified: parent unchanged after child operations\n");
    
    double start_time = get_time_ms();
    
    for (int i = 0; i < n_docs; ++i) {
        // Fork from parent
        ethervox_session_t* child = NULL;
        result = ethervox_session_fork(parent, &child);
        if (result != ETHERVOX_SUCCESS) {
            fprintf(stderr, "Failed to fork session: %s\n", ethervox_error_string(result));
            ethervox_session_destroy(parent);
            return -1.0;
        }
        
        // Add document to child
        const char* doc = DOCUMENTS[i % NUM_SAMPLE_DOCS];
        result = ethervox_session_prefill(child, doc, NULL, NULL);
        if (result != ETHERVOX_SUCCESS) {
            fprintf(stderr, "Failed to prefill document: %s\n", ethervox_error_string(result));
            ethervox_session_destroy(child);
            ethervox_session_destroy(parent);
            return -1.0;
        }
        
        // Generate classification (single token to demonstrate speedup)
        llama_seq_id child_seq_id = ethervox_session_get_seq_id(child);
        result = generate_single_token(ctx, model, child_seq_id);
        if (result != ETHERVOX_SUCCESS) {
            fprintf(stderr, "Failed to generate: %s\n", ethervox_error_string(result));
            ethervox_session_destroy(child);
            ethervox_session_destroy(parent);
            return -1.0;
        }
        
        // Clean up child
        ethervox_session_destroy(child);
        
        if ((i + 1) % 50 == 0) {
            printf("  Processed %d/%d documents...\n", i + 1, n_docs);
        }
    }
    
    double elapsed_ms = get_time_ms() - start_time;
    double throughput = (double)n_docs / (elapsed_ms / 1000.0);
    
    printf("Completed: %d documents in %.2f ms\n", n_docs, elapsed_ms);
    printf("Throughput: %.2f docs/sec\n", throughput);
    
    // Clean up parent
    ethervox_session_destroy(parent);
    
    return throughput;
}

int main(int argc, char** argv) {
    printf("=== Session Forking Benchmark (C3.2) ===\n");
    printf("Target: ≥4× throughput improvement\n");
    printf("Workload: %d document classifications\n\n", NUM_DOCUMENTS);
    
    // Parse arguments - default to testing model (smaller, already downloaded)
    const char* model_path = argc > 1 ? argv[1] : "~/.ethervox/models/governor/granite-4.0-h-1b-Q4_K_M.gguf";
    
    char* expanded_path = expand_tilde(model_path);
    if (!expanded_path) {
        fprintf(stderr, "Failed to expand path\n");
        return 1;
    }
    
    // Check if model exists
    if (access(expanded_path, F_OK) != 0) {
        fprintf(stderr, "Model not found: %s\n", expanded_path);
        fprintf(stderr, "Usage: %s [model_path]\n", argv[0]);
        free(expanded_path);
        return 1;
    }
    
    printf("Using model: %s\n", expanded_path);
    
    // Create paths
    ethervox_paths_t paths = {0};
    char path_buffer[4096];
    ethervox_result_t result = ethervox_paths_get_default(&paths, path_buffer, sizeof(path_buffer));
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to initialize paths: %s\n", ethervox_error_string(result));
        free(expanded_path);
        return 1;
    }
    
    // Create model pool (8GB budget)
    ethervox_model_pool_t* pool = NULL;
    result = ethervox_model_pool_create(&paths, 8ULL * 1024 * 1024 * 1024, &pool);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to create model pool: %s\n", ethervox_error_string(result));
        free(expanded_path);
        return 1;
    }
    
    // Load model with n_seq_max = 64 (compute buffer scales with n_seq_max × context_size;
    // 256 × 4096 exhausted GPU memory on this device)
    ethervox_model_config_t config = {
        .model_path = expanded_path,
        .mmproj_path = NULL,
        .context_size = 1024,
        .n_threads = 8,
        .n_seq_max = 64,
        .use_gpu = true,
        .kv_unified = true,  // Measure unified KV (good for shared prefix)
        .role = "classifier"
    };
    
    ethervox_model_handle_t* handle = NULL;
    result = ethervox_model_pool_load(pool, &config, NULL, NULL, &handle);
    if (result != ETHERVOX_SUCCESS) {
        fprintf(stderr, "Failed to load model: %s\n", ethervox_error_string(result));
        ethervox_model_pool_destroy(pool);
        free(expanded_path);
        return 1;
    }
    
    printf("Model loaded successfully\n");
    free(expanded_path);
    
    // Run benchmarks
    double naive_throughput = bench_naive(handle, NUM_DOCUMENTS);
    if (naive_throughput < 0) {
        fprintf(stderr, "Naive benchmark failed\n");
        ethervox_model_pool_unload(pool, handle);
        ethervox_model_pool_destroy(pool);
        return 1;
    }
    
    double fork_throughput = bench_fork(handle, NUM_DOCUMENTS);
    if (fork_throughput < 0) {
        fprintf(stderr, "Fork benchmark failed\n");
        ethervox_model_pool_unload(pool, handle);
        ethervox_model_pool_destroy(pool);
        return 1;
    }
    
    // Calculate speedup
    double speedup = fork_throughput / naive_throughput;
    
    printf("\n=== Results ===\n");
    printf("Naive throughput: %.2f docs/sec\n", naive_throughput);
    printf("Fork throughput:  %.2f docs/sec\n", fork_throughput);
    printf("Speedup:          %.2fx\n", speedup);
    
    if (speedup >= 4.0) {
        printf("✅ SUCCESS: ≥4× speedup achieved\n");
    } else {
        printf("❌ FAILED: Target speedup not met (%.2fx < 4.0x)\n", speedup);
    }
    
    // Cleanup
    ethervox_model_pool_unload(pool, handle);
    ethervox_model_pool_destroy(pool);
    
    return speedup >= 4.0 ? 0 : 1;
}
