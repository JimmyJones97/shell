//
// Created by dalunlun on 2019/8/14.
//

#ifndef SHELLAPP_UTIL_H
#define SHELLAPP_UTIL_H

#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include "Dalvik.h"

using namespace std;

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,__FUNCTION__,__VA_ARGS__) // 定义LOGD类型
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,__FUNCTION__,__VA_ARGS__) // 定义LOGE类型

#define BASE_10 10

jobject getAppContext(JNIEnv* env);

jobject getAssetsManager(JNIEnv*env);

const string getCUP_ABI();

jobject getClassLoader(JNIEnv*env);

bool HookNativeInline(const char *soPath, const char *signature, void *my_func, void **ori_func);

bool HookNativeAnonymityInline(const char *soPath, const uintptr_t addr, void *my_func,
                               void **ori_func);

bool GetSymbolByDLOpen(const char *soPath, const char *signature, void **result);

bool
HookJava(JNIEnv *env, const char *clazzPath, const char *methodName, const char *methodSignature,
         const void *my_func, jmethodID *ori_func);

vector<string> split(const string& str, const char* delimiter);

#endif //SHELLAPP_UTIL_H
