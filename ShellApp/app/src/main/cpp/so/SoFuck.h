//
// Created by dalunlun on 2019/8/14.
//

#ifndef SHELLAPP_SOFUCK_H
#define SHELLAPP_SOFUCK_H

#include <jni.h>
#include <string>
#include <set>
#include "../file/ConfigFileProxy.h"
#include "SoFile.h"

using namespace std;


struct SoFucker {
    JNIEnv *env;

    string fakeLibSoName;
    set<string> libNameSet;
    string fakeLibDir;
};

void initSoFucker(SoFucker **ppSoFucker, const ConfigFileProxy *pConfigFileProxy);

void updateNativeLibraryDirectories(JNIEnv *env, const string &libDirPath);

SoInfo *myFindLibrary(const char *name);

void hookFindLibrary();

bool mySoInfoLinkImage(SoInfo *soInfo);

void hookSoInfoLinkImage();

bool mySoInfoFree(SoInfo *soInfo);

void hookSoInfoFree();

#endif //SHELLAPP_SOFUCK_H
