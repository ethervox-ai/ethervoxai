/**
 * @file model_management_jni.c
 * @brief JNI bridge for model management
 *
 * Copyright (c) 2024-2025 EthervoxAI Team
 *
 * This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#include "ethervox/model_downloader.h"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ModelMgmtJNI", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ModelMgmtJNI", __VA_ARGS__)

// Helper to create Java string from C string
static jstring create_jstring(JNIEnv* env, const char* str) {
    if (!str) return NULL;
    return (*env)->NewStringUTF(env, str);
}

// Helper to create ModelInfo Java object
static jobject create_model_info_object(JNIEnv* env, const ethervox_model_info_t* info) {
    if (!info) return NULL;
    
    // Find ModelInfo class
    jclass modelInfoClass = (*env)->FindClass(env, "com/droid/ethervox_core/ModelInfo");
    if (!modelInfoClass) {
        LOGE("Failed to find ModelInfo class");
        return NULL;
    }
    
    // Find ModelType enum
    jclass modelTypeClass = (*env)->FindClass(env, "com/droid/ethervox_core/ModelType");
    if (!modelTypeClass) {
        LOGE("Failed to find ModelType class");
        (*env)->DeleteLocalRef(env, modelInfoClass);
        return NULL;
    }
    
    // Get ModelType.fromInt method
    jmethodID fromIntMethod = (*env)->GetStaticMethodID(env, modelTypeClass,
        "fromInt", "(I)Lcom/droid/ethervox_core/ModelType;");
    if (!fromIntMethod) {
        LOGE("Failed to find ModelType.fromInt method");
        (*env)->DeleteLocalRef(env, modelInfoClass);
        (*env)->DeleteLocalRef(env, modelTypeClass);
        return NULL;
    }
    
    // Get ModelType enum value
    jobject modelTypeObj = (*env)->CallStaticObjectMethod(env, modelTypeClass,
        fromIntMethod, (jint)info->type);
    
    // Find ModelStatus enum
    jclass modelStatusClass = (*env)->FindClass(env, "com/droid/ethervox_core/ModelStatus");
    if (!modelStatusClass) {
        LOGE("Failed to find ModelStatus class");
        (*env)->DeleteLocalRef(env, modelInfoClass);
        (*env)->DeleteLocalRef(env, modelTypeClass);
        (*env)->DeleteLocalRef(env, modelTypeObj);
        return NULL;
    }
    
    // Get ModelStatus.fromInt method
    jmethodID statusFromIntMethod = (*env)->GetStaticMethodID(env, modelStatusClass,
        "fromInt", "(I)Lcom/droid/ethervox_core/ModelStatus;");
    if (!statusFromIntMethod) {
        LOGE("Failed to find ModelStatus.fromInt method");
        (*env)->DeleteLocalRef(env, modelInfoClass);
        (*env)->DeleteLocalRef(env, modelTypeClass);
        (*env)->DeleteLocalRef(env, modelTypeObj);
        (*env)->DeleteLocalRef(env, modelStatusClass);
        return NULL;
    }
    
    // Get ModelStatus enum value
    jobject modelStatusObj = (*env)->CallStaticObjectMethod(env, modelStatusClass,
        statusFromIntMethod, (jint)info->status);
    
    // Find ModelInfo constructor
    jmethodID constructor = (*env)->GetMethodID(env, modelInfoClass, "<init>",
        "(Lcom/droid/ethervox_core/ModelType;"
        "Lcom/droid/ethervox_core/ModelStatus;"
        "Ljava/lang/String;"
        "Ljava/lang/String;"
        "J"  // sizeBytes
        "J"  // downloadedBytes
        "F"  // downloadProgress
        "Z"  // isDefault
        "Ljava/lang/String;"
        "Ljava/lang/String;)V");
    
    if (!constructor) {
        LOGE("Failed to find ModelInfo constructor");
        (*env)->DeleteLocalRef(env, modelInfoClass);
        (*env)->DeleteLocalRef(env, modelTypeClass);
        (*env)->DeleteLocalRef(env, modelTypeObj);
        (*env)->DeleteLocalRef(env, modelStatusClass);
        (*env)->DeleteLocalRef(env, modelStatusObj);
        return NULL;
    }
    
    // Create Java strings
    jstring pathStr = create_jstring(env, info->path);
    jstring nameStr = create_jstring(env, info->name);
    jstring descStr = create_jstring(env, info->description);
    jstring urlStr = create_jstring(env, info->url);
    
    // Create ModelInfo object
    jobject modelInfoObj = (*env)->NewObject(env, modelInfoClass, constructor,
        modelTypeObj,
        modelStatusObj,
        pathStr,
        nameStr,
        (jlong)info->size_bytes,
        (jlong)info->downloaded_bytes,
        (jfloat)info->download_progress,
        (jboolean)info->is_default,
        descStr,
        urlStr
    );
    
    // Cleanup
    (*env)->DeleteLocalRef(env, modelInfoClass);
    (*env)->DeleteLocalRef(env, modelTypeClass);
    (*env)->DeleteLocalRef(env, modelTypeObj);
    (*env)->DeleteLocalRef(env, modelStatusClass);
    (*env)->DeleteLocalRef(env, modelStatusObj);
    (*env)->DeleteLocalRef(env, pathStr);
    (*env)->DeleteLocalRef(env, nameStr);
    (*env)->DeleteLocalRef(env, descStr);
    (*env)->DeleteLocalRef(env, urlStr);
    
    return modelInfoObj;
}

JNIEXPORT jstring JNICALL
Java_com_droid_ethervox_1core_NativeLib_getModelBaseDir(
    JNIEnv* env, jobject thiz) {
    (void)thiz;
    
    char base_dir[512];
    if (ethervox_is_error(ethervox_model_get_base_dir(base_dir, sizeof(base_dir)))) {
        return NULL;
    }
    
    return create_jstring(env, base_dir);
}

JNIEXPORT jobject JNICALL
Java_com_droid_ethervox_1core_NativeLib_checkModelStatus(
    JNIEnv* env, jobject thiz, jint type, jstring modelName) {
    (void)thiz;
    
    const char* name = NULL;
    if (modelName != NULL) {
        name = (*env)->GetStringUTFChars(env, modelName, NULL);
    }
    
    LOGI("checkModelStatus called with type=%d, modelName=%s", type, name ? name : "null");
    
    ethervox_model_info_t info;
    memset(&info, 0, sizeof(info));
    
    ethervox_model_status_t status = ethervox_model_check_status(
        (ethervox_model_type_t)type, name, &info);
    
    LOGI("checkModelStatus returned: type=%d, status=%d, path=%s", info.type, info.status, info.path);
    
    if (modelName != NULL) {
        (*env)->ReleaseStringUTFChars(env, modelName, name);
    }
    
    // The C function fills the info structure completely, including status
    // Always return a ModelInfo object, even for NOT_FOUND
    // This allows Kotlin code to distinguish between NOT_FOUND and other statuses
    return create_model_info_object(env, &info);
}

JNIEXPORT jobject JNICALL
Java_com_droid_ethervox_1core_NativeLib_getDefaultModel(
    JNIEnv* env, jobject thiz, jint type) {
    (void)thiz;
    
    ethervox_model_info_t info;
    memset(&info, 0, sizeof(info));
    
    ethervox_result_t result = ethervox_model_get_default((ethervox_model_type_t)type, &info);
    if (ethervox_is_error(result)) {
        return NULL;
    }
    
    return create_model_info_object(env, &info);
}

JNIEXPORT jobjectArray JNICALL
Java_com_droid_ethervox_1core_NativeLib_listModels(
    JNIEnv* env, jobject thiz, jint type) {
    (void)thiz;
    
    ethervox_model_info_t* models = NULL;
    uint32_t count = 0;
    
    ethervox_result_t result = ethervox_model_list((ethervox_model_type_t)type, &models, &count);
    if (ethervox_is_error(result) || count == 0) {
        // Return empty array
        jclass modelInfoClass = (*env)->FindClass(env, "com/droid/ethervox_core/ModelInfo");
        return (*env)->NewObjectArray(env, 0, modelInfoClass, NULL);
    }
    
    // Create Java array
    jclass modelInfoClass = (*env)->FindClass(env, "com/droid/ethervox_core/ModelInfo");
    jobjectArray modelArray = (*env)->NewObjectArray(env, count, modelInfoClass, NULL);
    
    // Populate array
    for (uint32_t i = 0; i < count; i++) {
        jobject modelInfoObj = create_model_info_object(env, &models[i]);
        if (modelInfoObj) {
            (*env)->SetObjectArrayElement(env, modelArray, i, modelInfoObj);
            (*env)->DeleteLocalRef(env, modelInfoObj);
        }
    }
    
    // Free native array
    free(models);
    
    return modelArray;
}

JNIEXPORT jint JNICALL
Java_com_droid_ethervox_1core_NativeLib_downloadModel(
    JNIEnv* env, jobject thiz, jint type, jstring modelName) {
    (void)thiz;
    
    const char* name = (*env)->GetStringUTFChars(env, modelName, NULL);
    
    ethervox_result_t result = ethervox_model_download(
        (ethervox_model_type_t)type, name, NULL, NULL);
    
    (*env)->ReleaseStringUTFChars(env, modelName, name);
    
    return (jint)result;
}

JNIEXPORT jint JNICALL
Java_com_droid_ethervox_1core_NativeLib_cancelModelDownload(
    JNIEnv* env, jobject thiz, jint type, jstring modelName) {
    (void)thiz;
    
    const char* name = (*env)->GetStringUTFChars(env, modelName, NULL);
    
    ethervox_result_t result = ethervox_model_cancel_download(
        (ethervox_model_type_t)type, name);
    
    (*env)->ReleaseStringUTFChars(env, modelName, name);
    
    return (jint)result;
}

JNIEXPORT jint JNICALL
Java_com_droid_ethervox_1core_NativeLib_deleteModel(
    JNIEnv* env, jobject thiz, jint type, jstring modelName) {
    (void)thiz;
    
    const char* name = (*env)->GetStringUTFChars(env, modelName, NULL);
    
    ethervox_result_t result = ethervox_model_delete(
        (ethervox_model_type_t)type, name);
    
    (*env)->ReleaseStringUTFChars(env, modelName, name);
    
    return (jint)result;
}

JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_verifyModel(
    JNIEnv* env, jobject thiz, jint type, jstring modelPath) {
    (void)thiz;
    
    const char* path = (*env)->GetStringUTFChars(env, modelPath, NULL);
    
    bool result = ethervox_model_verify(
        (ethervox_model_type_t)type, path);
    
    (*env)->ReleaseStringUTFChars(env, modelPath, path);
    
    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL
Java_com_droid_ethervox_1core_NativeLib_getModelDiskUsage(
    JNIEnv* env, jobject thiz) {
    (void)env;
    (void)thiz;
    
    uint64_t bytes_used = 0;
    ethervox_model_get_disk_usage(&bytes_used);
    
    return (jlong)bytes_used;
}

JNIEXPORT jboolean JNICALL
Java_com_droid_ethervox_1core_NativeLib_checkModelDiskSpace(
    JNIEnv* env, jobject thiz, jint type, jstring modelName) {
    (void)thiz;
    
    const char* name = (*env)->GetStringUTFChars(env, modelName, NULL);
    
    bool result = ethervox_model_check_disk_space(
        (ethervox_model_type_t)type, name);
    
    (*env)->ReleaseStringUTFChars(env, modelName, name);
    
    return result ? JNI_TRUE : JNI_FALSE;
}
