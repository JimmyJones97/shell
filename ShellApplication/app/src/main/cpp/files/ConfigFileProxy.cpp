//
// Created by ChenD on 2019/9/9.
//

#include "ConfigFileProxy.h"
#include "../common/Util.h"
#include "FileUtil.h"
#include "../zip/zip.h"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

extern ConfigFileProxy *gConfigFileProxy;

void initConfigFileProxy(ConfigFileProxy **ppConfigFileProxy, JNIEnv *env) {
    LOGD("start, initConfigFileProxy(ConfigFileProxy **ppConfigFileProxy, JNIEnv *env)");
    auto *proxy = new ConfigFileProxy();
    proxy->env = env;
    proxy->filePath = getDataDir(env) + "/" + proxy->configFileName;
    *ppConfigFileProxy = proxy;
    LOGD("finish, initConfigFileProxy(ConfigFileProxy **ppConfigFileProxy, JNIEnv *env)");
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

void
readSizeAndOff(ifstream &reader, size_t *offset, size_t *pRetSize, size_t *pRetOffset, char *buf) {
    reader.seekg(*offset, ios::beg);
    reader.read(buf, SIZE_OF_DOUBLE_UINT32);
    *offset += SIZE_OF_DOUBLE_UINT32;
    *pRetSize = *(size_t *) buf;
    *pRetOffset = *(size_t *) (buf + 4);
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
         (void*)*ppUnzipDataBuf, *pBufSize);
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
    LOGD("             const char **ppLibSoBuf: %p, size_t *bufSize: %u)",(void*) *ppLibSoBuf, *bufSize);

}

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
