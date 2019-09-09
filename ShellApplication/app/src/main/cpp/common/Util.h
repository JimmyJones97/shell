//
// Created by ChenD on 2019/9/9.
//

#ifndef SHELLAPPLICATION_UTIL_H
#define SHELLAPPLICATION_UTIL_H

#include <android/log.h>
#include <jni.h>
#include <string>

using namespace std;

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,__FUNCTION__,__VA_ARGS__) // 定义LOGD类型
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,__FUNCTION__,__VA_ARGS__) // 定义LOGE类型

jobject getAppContext(JNIEnv* env);

jobject getAssetsManager(JNIEnv*env);

const string getCUP_ABI();

jobject getClassLoader(JNIEnv*env);

bool HookNativeInline(const char *soPath, const char *signature, void *myFunc, void **oriFunc);

bool HookNativeAnonymityInline(const char *soPath, const uintptr_t addr, void *myFunc,
                               void **oriFunc);

bool
HookJava(JNIEnv *env, const char *clazzPath, const char *methodName, const char *methodSignature,
         const void *myFunc, jmethodID *oriFunc);

#endif //SHELLAPPLICATION_UTIL_H
