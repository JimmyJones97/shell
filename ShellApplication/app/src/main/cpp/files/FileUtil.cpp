//
// Created by ChenD on 2019/9/9.
//

#include <unistd.h>
#include "FileUtil.h"
#include "../common/Util.h"
#include "ConfigFileProxy.h"
#include <vector>
#include <sys/stat.h>
#include <android/asset_manager_jni.h>
#include <fstream>

const string &getDataDir(JNIEnv *env) {
    static string data;
    if (data.empty()) {
        data = getBaseFilesDir(env) + "/data";
        LOGD("getDataDir success first....");
        LOGD("dataDir: %s", data.data());
    }
    return data;
}

const string &getBaseFilesDir(JNIEnv *env) {
    static string baseDir;
    if (baseDir.empty()) {
        jobject oContext = getAppContext(env);
        jclass cContext = (*env).GetObjectClass(oContext);
        jmethodID mGetFilesDir = (*env).GetMethodID(cContext, "getFilesDir", "()Ljava/io/File;");
        jobject oFile = (*env).CallObjectMethod(oContext, mGetFilesDir);
        assert(oFile != nullptr);
        jclass cFile = (*env).GetObjectClass(oFile);
        jmethodID mGetCanonicalPath = (*env).GetMethodID(cFile, "getCanonicalPath",
                                                         "()Ljava/lang/String;");
        auto oPath = reinterpret_cast<jstring >((*env).CallObjectMethod(oFile, mGetCanonicalPath));
        assert(oPath != nullptr);
        baseDir = (*env).GetStringUTFChars(oPath, JNI_FALSE);

        LOGD("getBaseFilesDir success first....");
        LOGD("baseDir: %s", baseDir.data());
        (*env).DeleteLocalRef(cContext);
        (*env).DeleteLocalRef(cFile);
    }
    return baseDir;
}

const string &getDefaultLibDir(JNIEnv *env) {
    static string lib;
    if (lib.empty()) {
        lib = getBaseFilesDir(env);
        size_t end_index = lib.rfind("/");
        assert(end_index != lib.npos);
        lib = lib.substr(0, end_index) + "/lib";
        LOGD("getDefaultLibDir success first....");
        LOGD("libDirPath: %s", lib.data());
    }
    return lib;
}

const string &getFakeLibDir(JNIEnv *env) {
    static string lib;
    if (lib.empty()) {
        lib = getBaseFilesDir(env) + "/lib";
        LOGD("getFakeLibDir success first....");
        LOGD("libDirPath: %s", lib.data());
    }
    return lib;
}

const string &getOdexDir(JNIEnv *env) {
    static string odex;
    if (odex.empty()) {
        odex = getBaseFilesDir(env) + "/odex";
        LOGD("getOdexDir success first....");
        LOGD("odexDir: %s", odex.data());
    }
    return odex;
}

const string &getDexDir(JNIEnv *env) {
    static string dex;
    if (dex.empty()) {
        dex = getBaseFilesDir(env) + "/dex";
        LOGD("getDexDir success first....");
        LOGD("dexDir: %s", dex.data());
    }
    return dex;
}

void buildFileSystem(const ConfigFileProxy *pConfigFileProxy) {
    LOGD("start, buildFileSystem()");
    vector<string> path = {
            getBaseFilesDir(pConfigFileProxy->env),
            getDataDir(pConfigFileProxy->env),
            getOdexDir(pConfigFileProxy->env),
            getDexDir(pConfigFileProxy->env)
    };

    for (auto &p : path) {
        if (0 != access(p.data(), R_OK) &&
            0 != mkdir(p.data(), S_IRWXU | S_IRWXG | S_IRWXO)) {
            // NOLINT(hicpp-signed-bitwise)
            // if this folder not exist, create a new one.
            LOGE("create dir failed: %s", p.data());
            throw runtime_error("create dir failed");
        }
    }

    // copy file from assets
    copyFileFromAssets(pConfigFileProxy->env, pConfigFileProxy->configFileName,
                       pConfigFileProxy->filePath);
    LOGD("finish, buildFileSystem()");
}

void copyFileFromAssets(JNIEnv *env, const string &configFileName, const string &dstFilePath) {
    LOGD("start, copyFileFromAssets(JNIEnv *env, const string &configFileName, const string &dstFilePath)");
    LOGD("copyFileFromAssets start, write %s to %s", configFileName.data(), dstFilePath.data());
    jobject oAssetsManager = getAssetsManager(env);
    AAssetManager *assetManager = AAssetManager_fromJava(env, oAssetsManager);
    assert(assetManager != nullptr);
    LOGD("Get AssetManager success...");

    AAsset *asset = AAssetManager_open(assetManager, configFileName.data(), AASSET_MODE_BUFFER);
    auto fileSize = AAsset_getLength(asset);
    const char *fileBuf = reinterpret_cast<const char *>(AAsset_getBuffer(asset));
    LOGD("srcPath: %s, fileSize: %ld", configFileName.data(), fileSize);

    ofstream writer(dstFilePath.data(), ios::binary);
    writer.write(fileBuf, fileSize);
    writer.close();
    LOGD("finish, copyFileFromAssets(JNIEnv *env, const string &configFileName, const string &dstFilePath)");
}
