//
// Created by ChenD on 2019/9/9.
//

#ifndef SHELLAPPLICATION_DEXFUCK_H
#define SHELLAPPLICATION_DEXFUCK_H

#include <map>
#include <string>
#include <jni.h>
#include "../files/ConfigFileProxy.h"
#include "../common/Common.h"

using namespace std;

struct RawDexFile;

struct CodeItemData {
    uint16_t registersSize;
    uint16_t insSize;
    uint16_t outSize;
    uint16_t triesSize;
    uint32_t debugInfoOff;
    uint32_t insnsSize;
    uint16_t *insns;
    uint8_t *triesAndHandlersBuf;
};

struct DexFucker {
    JNIEnv *env;
    string fakeClassesDexName;

    char *dexBuf;
    size_t dexLen;

    const char *codeItemBuf;
    size_t codeItemLen;
    map<string, CodeItemData *> codeItem;
};

void initDexFucker(DexFucker **ppDexFucker, const ConfigFileProxy *pConfigFileProxy);

void loadDexFromMemoryDalvik();

int myDvmRawDexFileOpen(const char *fileName, const char *odexOutputName, RawDexFile **ppRawDexFile,
                        bool isBootstrap);

void hookDvmRawDexFileOpen();

RawDexFile *createRawDexFileFromMemoryDalvik();

const CodeItemData *getCodeItemByKey(const string &key);

#endif //SHELLAPPLICATION_DEXFUCK_H
