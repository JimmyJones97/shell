//
// Created by dalunlun on 2019/8/14.
//

#include <unistd.h>
#include <sys/stat.h>
#include <android/asset_manager_jni.h>
#include "ConfigFileProxy.h"
#include "../zip/zip.h"


extern ConfigFileProxy *gConfigFileProxy;

void initConfigFile() {
    LOGD("start, initConfigFile()");
    ifstream reader(gConfigFileProxy->filePath, ios::binary);

    // get config file's size
    reader.seekg(0, ios::end);
    gConfigFileProxy->fileSize = static_cast<size_t>(reader.tellg());
    LOGD("filePath: %s, fileSize: %d", gConfigFileProxy->filePath.data(),
         gConfigFileProxy->fileSize);
    char buf[BUFSIZ];

    reader.seekg(0, ios::beg);
    size_t offset = 0;

    readStr(reader, &offset, &gConfigFileProxy->srcApkApplicationName, buf);
    LOGD("srcApkApplicationName: %s", gConfigFileProxy->srcApkApplicationName.data());

    readStr(reader, &offset, &gConfigFileProxy->fakeClassesDexName, buf);
    LOGD("fakeClassesDexName: %s", gConfigFileProxy->fakeClassesDexName.data());

    readStr(reader, &offset, &gConfigFileProxy->fakeLibSoName, buf);
    LOGD("fakeLibSoName: %s", gConfigFileProxy->fakeLibSoName.data());

    readSizeAndOff(reader, &offset, &gConfigFileProxy->srcLibNameSize,
                   &gConfigFileProxy->srcLibNameOff, buf);
    LOGD("srcLibNameSize: %d, srcLibNameOff: %d",
         gConfigFileProxy->srcLibNameSize, gConfigFileProxy->srcLibNameOff);

    readSizeAndOff(reader, &offset, &gConfigFileProxy->srcLibZipSize,
                   &gConfigFileProxy->srcLibZipOff, buf);
    LOGD("srcLibZipSize: %d, srcLibZipOff: %d",
         gConfigFileProxy->srcLibZipSize, gConfigFileProxy->srcLibZipOff);

    readSizeAndOff(reader, &offset, &gConfigFileProxy->srcClassesDexSize,
                   &gConfigFileProxy->srcClassesDexOff, buf);
    LOGD("srcClassesDexSize: %d, srcClassesDexOff: %d",
         gConfigFileProxy->srcClassesDexSize, gConfigFileProxy->srcClassesDexOff);

    readSizeAndOff(reader, &offset, &gConfigFileProxy->srcCodeItemSize,
                   &gConfigFileProxy->srcCodeItemOff, buf);
    LOGD("srcCodeItemSize: %d, srcCodeItemOff: %d",
         gConfigFileProxy->srcCodeItemSize, gConfigFileProxy->srcCodeItemOff);

    reader.close();
    LOGD("finish, initConfigFile()");
}


void readStr(ifstream &reader, size_t *offset, string *pRetString, char *buf) {
    size_t size;
    size_t off;
    readSizeAndOff(reader, offset, &size, &off, buf);
    reader.seekg(off, ios::beg);
    reader.read(buf, size);
    buf[size] = '\0';
    *pRetString = buf;
}

void readSizeAndOff(ifstream &reader, size_t *offset, size_t *pRetSize, size_t *pRetOffset,
                    char *buf) {
    reader.seekg(*offset, ios::beg);
    reader.read(buf, SIZE_OF_DOUBLE_UINT32);
    *offset += SIZE_OF_DOUBLE_UINT32;
    *pRetSize = *(size_t *) buf;
    *pRetOffset = *(size_t *) (buf + 4);
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

const string &getDataDir(JNIEnv *env) {
    static string data;
    if (data.empty()) {
        data = getBaseFilesDir(env) + "/data";
        LOGD("getDataDir success first....");
        LOGD("dataDir: %s", data.data());
    }
    return data;
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

const string &getFakeLibDir(JNIEnv *env){
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

void buildFileSystem() {
    LOGD("start, buildFileSystem()");
    vector<string> path = {
            getBaseFilesDir(gConfigFileProxy->env),
            getDataDir(gConfigFileProxy->env),
            getOdexDir(gConfigFileProxy->env),
            getDexDir(gConfigFileProxy->env)
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
    copyFileFromAssets(gConfigFileProxy->env, gConfigFileProxy->configFileName,
                       gConfigFileProxy->filePath);
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

void initConfigFileProxy(ConfigFileProxy **ppConfigFileProxy, JNIEnv *env) {
    LOGD("start, initConfigFileProxy(ConfigFileProxy **ppConfigFileProxy, JNIEnv *env)");
    auto *proxy = new ConfigFileProxy();
    proxy->env = env;
    proxy->filePath = getDataDir(env) + "/" + proxy->configFileName;
    *ppConfigFileProxy = proxy;
    LOGD("finish, initConfigFileProxy(ConfigFileProxy **ppConfigFileProxy, JNIEnv *env)");
}

void buildFile(const string &srcPath, const string &desPath, unsigned int off, size_t length) {
    ofstream writer(desPath, ios::binary);
    ifstream reader(srcPath, ios::binary);
    char buf[BUFSIZ];

    LOGD("start write files...   %s", desPath.data());
    reader.seekg(off, ios::beg);
    while (length > 0) {
        size_t realReadSize = length >= BUFSIZ ? BUFSIZ : length;
        length -= realReadSize;
        reader.read(buf, realReadSize);
        writer.write(buf, realReadSize);
    }

    LOGD("finish write files...   %s", desPath.data());
    reader.close();
    writer.flush();
    writer.close();
}

//void buildLibFile() {
//    buildZipFile(gConfigFileProxy->filePath, getDefaultLibDir(gConfigFileProxy->env), getCUP_ABI(),
//                 gConfigFileProxy->srcLibZipOff, gConfigFileProxy->srcLibZipSize);
//}

void buildZipFile(const string &srcPath, const string &desPath, const string &entryName,
                  unsigned int off, size_t length) {
    if (0 != access(desPath.data(), R_OK) &&
        0 != mkdir(desPath.data(), S_IRWXU | S_IRWXG | S_IRWXO)) {
        // NOLINT(hicpp-signed-bitwise)
        // if this folder not exist, create a new one.
        LOGE("create dir failed: %s", desPath.data());
        throw runtime_error("create dir failed");
    }
    LOGD("mkdir success: %s", desPath.data());
    LOGD("filePath: %s", srcPath.data());
    ifstream reader(srcPath, ios::binary);
    reader.seekg(off, ios_base::beg);
    char *buf = new char[length]();
    assert(buf != nullptr);
    reader.read(buf, length);
    reader.close();
    LOGD("read zip file to memory success...");

    zip_t *zip = zip_open_from_mem(buf, length, ZIP_DEFAULT_COMPRESSION_LEVEL);
    zip_entry_open(zip, entryName.data());
    LOGD("%s", zip->entry.name);
    if (0 != zip_extract_from_handle(zip, desPath.data(), nullptr, nullptr)) {
        free(buf);
        LOGE("zip_extract_from_handle error...");
        throw runtime_error("zip_extract_from_handle error...");
    }

    delete[]buf;
    LOGD("buildZipFile success...");
}

const char *getDataBuf(const size_t off, const size_t size) {
    char *buf = new char[size]();
    ifstream reader(gConfigFileProxy->filePath, ios::binary);
    reader.seekg(off, ios::beg);
    reader.read(buf, size);
    reader.close();
    LOGD("getDataBuf(size_t off: %d, size_t size: %d)", off, size);
    return buf;
}

void readZipDataBuf(const size_t off, const size_t size, const char *entryName,
                    const char **ppUnzipDataBuf, size_t *pBufSize) {
    const char *zipDataBuf = getDataBuf(off, size);
    zip_t *zipFile = zip_open_from_mem(zipDataBuf, size, ZIP_DEFAULT_COMPRESSION_LEVEL);
    zip_entry_open(zipFile, entryName);
    zip_entry_read(zipFile, (void **) ppUnzipDataBuf, pBufSize);
    assert(ppUnzipDataBuf != nullptr && *pBufSize != 0);
    LOGD("readZipDataBuf(const size_t off: %u, const size_t size: %u, const char *entryName: %s, ",
         off, size, entryName);
    LOGD("               const char **ppUnzipDataBuf: %p, size_t *pBufSize: %u)",
         *ppUnzipDataBuf, *pBufSize);
    delete zipDataBuf;
}

const char *getCodeItemBuf() {
    LOGD("getCodeItemBuf()");
    return getDataBuf(gConfigFileProxy->srcCodeItemOff, gConfigFileProxy->srcCodeItemSize);
}

const char *getClassesDexBuf() {
    LOGD("getClassesDexBuf()");
    return getDataBuf(gConfigFileProxy->srcClassesDexOff, gConfigFileProxy->srcClassesDexSize);
}

const char *getLibNameBuf() {
    LOGD("getLibNameBuf()");
    return getDataBuf(gConfigFileProxy->srcLibNameOff, gConfigFileProxy->srcLibNameSize);
}

void readLibSoBuf(const char *entryName, const char **ppLibSoBuf, size_t *bufSize) {
    readZipDataBuf(gConfigFileProxy->srcLibZipOff, gConfigFileProxy->srcLibZipSize,
                   entryName, ppLibSoBuf, bufSize);
    LOGD("readLibSoBuf(const char *entryName: %s, ", entryName);
    LOGD("             const char **ppLibSoBuf: %p, size_t *bufSize: %u)", *ppLibSoBuf, *bufSize);
}
