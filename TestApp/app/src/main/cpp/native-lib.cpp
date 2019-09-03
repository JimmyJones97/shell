#include <jni.h>
#include <string>

#include <android/log.h>
#include <dlfcn.h>

#include "include/AndHook.h"

using namespace std;

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,__FUNCTION__,__VA_ARGS__) // 定义LOGD类型
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,__FUNCTION__,__VA_ARGS__) // 定义LOGE类型



extern "C" JNIEXPORT jstring JNICALL
Java_com_chend_testapp_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {

    void* handle  = dlopen("/data/data/com.chend.testapp/lib/libhello.so", RTLD_NOW);
    assert(handle!= nullptr);
    void* symbol = dlsym(handle, "_Z5hellov");
    assert(symbol!= nullptr);

    const char* (*hello)() = nullptr;
    hello = reinterpret_cast<const char *(*)()>(symbol);


    std::string haha = "Hello from C++";
    haha+=hello();
    return env->NewStringUTF(haha.c_str());
}