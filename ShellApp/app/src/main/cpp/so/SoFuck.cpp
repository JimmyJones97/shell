//
// Created by dalunlun on 2019/8/14.
//

#include <dlfcn.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "SoFuck.h"
#include "../common/Util.h"
#include "../zip/zip.h"

extern SoFucker *gSoFucker;

bool (*oriSoInfoLinkImage)(SoInfo *soInfo) = nullptr;

bool mySoInfoLinkImage(SoInfo *soInfo) {
    LOGD("mySoInfoLinkImage: %s", soInfo->name);
    return oriSoInfoLinkImage(soInfo);
}

void hookSoInfoLinkImage() {
    static bool hookFlag = false;
    if (!hookFlag) {
        hookFlag = HookNativeAnonymityInline("/system/bin/linker",
                                             0x19C0,
                                             reinterpret_cast<void *>(mySoInfoLinkImage),
                                             reinterpret_cast<void **>(&oriSoInfoLinkImage));
    }
}

bool (*oriSoInfoFree)(SoInfo *soInfo) = nullptr;

bool mySoInfoFree(SoInfo *soInfo) {
    LOGD("mySoInfoFree: %s", soInfo->name);
    return oriSoInfoFree(soInfo);
}

void hookSoInfoFree() {
    static bool hookFlag = false;
    if (!hookFlag) {
        hookFlag = HookNativeAnonymityInline("/system/bin/linker",
                                             0x1894,
                                             reinterpret_cast<void *>(mySoInfoFree),
                                             reinterpret_cast<void **>(&oriSoInfoFree));
    }
}


SoInfo *(*oriFindLibrary)(const char *name) = nullptr;

SoInfo *myFindLibrary(const char *name) {
    LOGD("myFindLibrary: %s", name);
    /**
     * try to find library by original function,
     * when the so has loaded or the so doesn't local at default lib location,
     * it will return non-null.
     */
    SoInfo *soInfo = oriFindLibrary(name);
    if (soInfo != nullptr) {
        LOGD("oriFindLibrary: %s", name);
        return soInfo;
    }

    // check the name
    string nameStr = name;
    size_t end_index = nameStr.rfind('/');
    if (end_index == nameStr.npos) {
        LOGE("can't find library: %s", name);
        return nullptr;
    }
    string soNameStr = nameStr.substr(end_index + 1);
    string soFileParentPath = nameStr.substr(0, end_index);
    auto it = gSoFucker->libNameSet.find(soNameStr);
    if (it != gSoFucker->libNameSet.end() &&
        soFileParentPath == getDefaultLibDir(gSoFucker->env) ) {
        LOGD("found lib( %s ) in memory...", name);
        string fakeLibSoPath = getDefaultLibDir(gSoFucker->env) + "/" + gSoFucker->fakeLibSoName;
        soInfo = oriFindLibrary(fakeLibSoPath.data());
        assert(soInfo != nullptr);

        // munmap fake lib so
        munmap(reinterpret_cast<void *>(soInfo->base), soInfo->size);

        const char *libSoBuf = nullptr;
        size_t bufSize = 0;
        readLibSoBuf((getCUP_ABI() + "/" + soNameStr).data(), &libSoBuf, &bufSize);
        openElf(libSoBuf, soNameStr, soInfo);
        LOGD("[ init_library base=0x%08x sz=0x%08x name='%s' ]",
             soInfo->base, soInfo->size, soInfo->name);

        hookSoInfoLinkImage();
        if (!oriSoInfoLinkImage(soInfo)) {
            munmap(reinterpret_cast<void *>(soInfo->base), soInfo->size);
            hookSoInfoFree();
            oriSoInfoFree(soInfo);
            LOGE("can't link so: %s", name);
            return nullptr;
        }
        LOGD("load so( %s ) from memory, finish...", name);
        return soInfo;
    }
    LOGE("can't find library: %s", name);
    return nullptr;
}

void hookFindLibrary() {
    static bool hookFlag = false;
    if (!hookFlag) {
        hookFlag = HookNativeAnonymityInline("/system/bin/linker",
                                             0x22B4,
                                             reinterpret_cast<void *>(myFindLibrary),
                                             reinterpret_cast<void **>(&oriFindLibrary));
    }

}

void initSoFucker(SoFucker **ppSoFucker, const ConfigFileProxy *pConfigFileProxy) {
    SoFucker *soFucker = new SoFucker();
    soFucker->env = pConfigFileProxy->env;
    soFucker->fakeLibSoName = pConfigFileProxy->fakeLibSoName;
    soFucker->fakeLibDir = getFakeLibDir(soFucker->env);

    updateNativeLibraryDirectories(soFucker->env, soFucker->fakeLibDir);
    const char *libNameBuf = getLibNameBuf();
    size_t *nameNum = (size_t *) libNameBuf;
    size_t cur_offset = 4;
    for (int i = 0; i < *nameNum; i++) {
        size_t nameLen = *(size_t *) (libNameBuf + cur_offset);
        cur_offset += 4;
        soFucker->libNameSet.insert(libNameBuf + cur_offset);
        LOGD("build empty so file: %s", libNameBuf + cur_offset);
        string fakeLibSoFilePath = soFucker->fakeLibDir + "/" + (libNameBuf + cur_offset);
        int fd = open(fakeLibSoFilePath.data(), O_RDWR | O_CREAT, S_IRWXU);
        assert(fd != -1);
        close(fd);
        cur_offset += nameLen;
    }
    delete libNameBuf;

    hookFindLibrary();
    *ppSoFucker = soFucker;
}

void updateNativeLibraryDirectories(JNIEnv *env, const string &libDirPath) {
    LOGD("start, updateNativeLibraryDirectories(JNIEnv *env, const string &libDirPath)");
    if (0 != access(libDirPath.data(), R_OK) &&
        0 != mkdir(libDirPath.data(), S_IRWXU | S_IRWXG | S_IRWXO)) {
        // NOLINT(hicpp-signed-bitwise)
        // if this folder not exist, create a new one.
        LOGE("create dir failed: %s", libDirPath.data());
        throw runtime_error("create dir failed");
    }

    jobject oClassLoader = getClassLoader(env);
    jclass cClassLoader = (*env).GetObjectClass(oClassLoader);
    jclass cBaseDexClassLoader = (*env).GetSuperclass(cClassLoader);
    jfieldID fPathList = (*env).GetFieldID(cBaseDexClassLoader, "pathList",
                                           "Ldalvik/system/DexPathList;");
    jobject oPathList = (*env).GetObjectField(oClassLoader, fPathList);
    LOGD("1");
    jclass cDexPathList = (*env).GetObjectClass(oPathList);
    jfieldID fNativeLibraryDirectories = (*env).GetFieldID(cDexPathList, "nativeLibraryDirectories",
                                                           "[Ljava/io/File;");
    auto oNativeLibraryDirectories = reinterpret_cast<jobjectArray >((*env).GetObjectField(
            oPathList, fNativeLibraryDirectories));
    LOGD("2");
    jsize oldLenNativeLibraryDirectories = (*env).GetArrayLength(oNativeLibraryDirectories);
    LOGD("oldLenNativeLibraryDirectories: %d", oldLenNativeLibraryDirectories);
    jclass cFile = (*env).FindClass("java/io/File");
    auto oNativeLibraryDirectoriesNew = (*env).NewObjectArray(oldLenNativeLibraryDirectories + 1,
                                                              cFile, nullptr);
    for (int i = 0; i < oldLenNativeLibraryDirectories; i++) {
        auto t = (*env).GetObjectArrayElement(oNativeLibraryDirectories, i);
        (*env).SetObjectArrayElement(oNativeLibraryDirectoriesNew, i, t);
    }
    LOGD("3");
    jmethodID mFile = (*env).GetMethodID(cFile, "<init>", "(Ljava/lang/String;)V");
    jstring filenameStr = (*env).NewStringUTF(libDirPath.data());
    jobject oFile = (*env).NewObject(cFile, mFile, filenameStr);
    (*env).SetObjectArrayElement(oNativeLibraryDirectoriesNew, oldLenNativeLibraryDirectories,
                                 oFile);
    LOGD("4");
    (*env).SetObjectField(oPathList, fNativeLibraryDirectories, oNativeLibraryDirectoriesNew);
    LOGD("5");
    LOGD("finish, updateNativeLibraryDirectories(JNIEnv *env, const string &libDirPath)");
}


