//
// Created by ChenD on 2019/9/9.
//

#ifndef SHELLAPPLICATION_SOFUCK_H
#define SHELLAPPLICATION_SOFUCK_H

#include <string>
#include <set>
#include <jni.h>
#include "../files/ConfigFileProxy.h"
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
#endif //SHELLAPPLICATION_SOFUCK_H
