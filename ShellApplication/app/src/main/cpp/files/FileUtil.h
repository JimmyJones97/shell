//
// Created by ChenD on 2019/9/9.
//

#ifndef SHELLAPPLICATION_FILEUTIL_H
#define SHELLAPPLICATION_FILEUTIL_H


#include <jni.h>
#include <string>
#include "ConfigFileProxy.h"

using namespace std;

const string &getDataDir(JNIEnv *env);

const string &getBaseFilesDir(JNIEnv *env);

const string &getDefaultLibDir(JNIEnv *env);

const string &getFakeLibDir(JNIEnv *env);

const string &getOdexDir(JNIEnv *env);

const string &getDexDir(JNIEnv *env);

void buildFileSystem(const ConfigFileProxy *pConfigFileProxy);

void copyFileFromAssets(JNIEnv *env, const string &configFileName, const string &dstFilePath);


#endif //SHELLAPPLICATION_FILEUTIL_H
