//
// Created by dalunlun on 2019/8/14.
//

#ifndef SHELLAPP_DEXFUCK_H
#define SHELLAPP_DEXFUCK_H

#include "../file/ConfigFileProxy.h"
#include "../common/Dalvik.h"
#include "VMInterpreter.h"
#include <map>
#include <string>
using namespace std;

struct CodeItemData{
    u2 registersSize;
    u2 insSize;
    u2 outSize;
    u2 triesSize;
    u4 debugInfoOff;
    u4 insnsSize;
    u2* insns;
    u1* triesAndHandlersBuf;
};

struct DexFucker{
    JNIEnv *env;
    string fakeClassesDexName;

    char* dexBuf;
    size_t dexLen;

    const char* codeItemBuf;
    size_t codeItemLen;
    map<string, CodeItemData*> codeItem;
};



void initDexFucker(DexFucker **ppDexFucker, const ConfigFileProxy *pConfigFileProxy);

void loadDexFromMemoryDalvik();

int myDvmRawDexFileOpen(const char *fileName, const char *odexOutputName, RawDexFile **ppRawDexFile,
                        bool isBootstrap);

void hookDvmRawDexFileOpen();

RawDexFile *createRawDexFileFromMemoryDalvik();

const CodeItemData* getCodeItem(const string &key);
#endif //SHELLAPP_DEXFUCK_H
