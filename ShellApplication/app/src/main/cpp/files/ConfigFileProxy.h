//
// Created by ChenD on 2019/9/9.
//

#ifndef SHELLAPPLICATION_CONFIGFILEPROXY_H
#define SHELLAPPLICATION_CONFIGFILEPROXY_H


#include <cstddef>
#include <jni.h>
#include <string>

using namespace std;

#define SIZE_OF_DOUBLE_UINT32 8

struct ConfigFileProxy {
    JNIEnv *env;
    string configFileName = "config.bin";

    string filePath;
    size_t fileSize = 0;

    string srcApkApplicationName;
    string fakeClassesDexName;
    string fakeLibSoName;

    size_t srcLibNameSize;
    size_t srcLibNameOff;

    size_t srcLibZipSize;
    size_t srcLibZipOff;

    size_t srcClassesDexSize;
    size_t srcClassesDexOff;

    size_t srcCodeItemSize;
    size_t srcCodeItemOff;
};

void initConfigFileProxy(ConfigFileProxy **ppConfigFileProxy, JNIEnv *env);

void initConfigFile();

inline void readStr(ifstream &reader, size_t *offset, string *pRetString, char *buf);

inline void
readSizeAndOff(ifstream &reader, size_t *offset, size_t *pRetSize, size_t *pRetOffset,
               char *buf);

void buildFile(const string &srcPath, const string &desPath, unsigned int off, size_t length);

void buildZipFile(const string &srcPath, const string &desPath, const string &entryName,
                  unsigned int off, size_t length);

const char *getDataBuf(const size_t off, const size_t size);

void readZipDataBuf(const size_t off, const size_t size, const char *entryName,
                    const char **ppUnzipDataBuf, size_t *pBufSize);

const char *getCodeItemBuf();

const char *getClassesDexBuf();

const char *getLibNameBuf();

void readLibSoBuf(const char *entryName, const char **ppLibSoBuf, size_t *bufSize);

#endif //SHELLAPPLICATION_CONFIGFILEPROXY_H
