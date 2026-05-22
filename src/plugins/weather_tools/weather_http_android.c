/**
 * @file weather_http_android.c
 * @brief Android-specific HTTP implementation for weather tools using JNI
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 * Licensed under CC BY-NC-SA 4.0
 */

#include "ethervox/weather_tools.h"
#include "ethervox/logging.h"
#include "ethervox/error.h"
#include <android/log.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "WeatherHTTP", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "WeatherHTTP", __VA_ARGS__)

// Global JVM reference (set during platform init)
extern JavaVM* g_jvm;

/**
 * @brief Make HTTP GET request using Android's Java HTTP client
 */
ethervox_result_t android_http_get_request(
    const char* url,
    char** response_out,
    char** error_message_out
) {
    if (!url || !response_out) {
        return ETHERVOX_ERROR_INVALID_ARGUMENT;
    }

    *response_out = NULL;
    if (error_message_out) {
        *error_message_out = NULL;
    }

    if (!g_jvm) {
        LOGE("JVM not initialized");
        if (error_message_out) {
            *error_message_out = strdup("JVM not available");
        }
        return ETHERVOX_ERROR_PLATFORM_INIT;
    }

    JNIEnv* env = NULL;
    int getEnvStatus = (*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6);
    
    bool needDetach = false;
    if (getEnvStatus == JNI_EDETACHED) {
        if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != 0) {
            LOGE("Failed to attach thread to JVM");
            if (error_message_out) {
                *error_message_out = strdup("Failed to attach thread");
            }
            return ETHERVOX_ERROR_PLATFORM_OPERATION_FAILED;
        }
        needDetach = true;
    }

    ethervox_result_t result = ETHERVOX_SUCCESS;

    // Find the EthervoxHttpClient helper class
    jclass httpClientClass = (*env)->FindClass(env, "ai/ethervox/friendomine/core/EthervoxHttpClient");
    if (!httpClientClass) {
        LOGE("Failed to find EthervoxHttpClient class");
        if (error_message_out) {
            *error_message_out = strdup("HTTP client class not found");
        }
        result = ETHERVOX_ERROR_NOT_FOUND;
        goto cleanup;
    }

    // Get the static httpGet method
    jmethodID httpGetMethod = (*env)->GetStaticMethodID(
        env, httpClientClass, "httpGet", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!httpGetMethod) {
        LOGE("Failed to find httpGet method");
        if (error_message_out) {
            *error_message_out = strdup("HTTP get method not found");
        }
        result = ETHERVOX_ERROR_NOT_FOUND;
        goto cleanup;
    }

    // Convert URL to jstring
    jstring jurl = (*env)->NewStringUTF(env, url);
    if (!jurl) {
        LOGE("Failed to create URL string");
        if (error_message_out) {
            *error_message_out = strdup("Failed to create URL");
        }
        result = ETHERVOX_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }

    // Call the Java method
    LOGI("Making HTTP GET request to: %s", url);
    jstring jresponse = (*env)->CallStaticObjectMethod(env, httpClientClass, httpGetMethod, jurl);
    (*env)->DeleteLocalRef(env, jurl);

    // Check for exceptions
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        LOGE("Exception during HTTP request");
        if (error_message_out) {
            *error_message_out = strdup("HTTP request failed");
        }
        result = ETHERVOX_ERROR_NETWORK;
        goto cleanup;
    }

    if (!jresponse) {
        LOGE("HTTP request returned null");
        if (error_message_out) {
            *error_message_out = strdup("HTTP request returned null");
        }
        result = ETHERVOX_ERROR_NETWORK;
        goto cleanup;
    }

    // Convert response to C string
    const char* responseChars = (*env)->GetStringUTFChars(env, jresponse, NULL);
    if (responseChars) {
        *response_out = strdup(responseChars);
        (*env)->ReleaseStringUTFChars(env, jresponse, responseChars);
        LOGI("HTTP request successful, response size: %zu bytes", strlen(*response_out));
    } else {
        LOGE("Failed to get response string");
        if (error_message_out) {
            *error_message_out = strdup("Failed to convert response");
        }
        result = ETHERVOX_ERROR_FAILED;
    }

    (*env)->DeleteLocalRef(env, jresponse);

cleanup:
    if (needDetach) {
        (*g_jvm)->DetachCurrentThread(g_jvm);
    }

    return result;
}
