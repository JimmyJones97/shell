//
// Created by ChenD on 2019/9/9.
//

#include <pthread.h>
#include "DexFuck.h"
#include "../common/Util.h"
#include "DexUtil.h"
#include "../files/FileUtil.h"


extern DexFucker *gDexFucker;


int
(*sysDvmRawDexFileOpen)(const char *fileName, const char *odexOutputName, RawDexFile **ppRawDexFile,
                        bool isBootstrap);

int myDvmRawDexFileOpen(const char *fileName, const char *odexOutputName, RawDexFile **ppRawDexFile,
                        bool isBootstrap) {

    LOGD("fileName: %s", fileName);
    if (strcmp(odexOutputName, getOdexDir(gDexFucker->env).data()) == 0) {
        LOGD("fake dex, ha ha ha...");
        *ppRawDexFile = createRawDexFileFromMemoryDalvik();
        LOGD("*ppRawDexFile: %p", *ppRawDexFile);

        return *ppRawDexFile == nullptr ? -1 : 0;
    }
    return sysDvmRawDexFileOpen(fileName, odexOutputName, ppRawDexFile, isBootstrap);
}

void hookDvmRawDexFileOpen() {
    static bool hookFlag = false;
    if (!hookFlag) {
        hookFlag = HookNativeInline("/system/lib/libdvm.so",
                                    "_Z17dvmRawDexFileOpenPKcS0_PP10RawDexFileb",
                                    reinterpret_cast<void *>(myDvmRawDexFileOpen),
                                    reinterpret_cast<void **>(&sysDvmRawDexFileOpen));
        hookFlag = true;
    }
}

void loadDexFromMemoryDalvik() {
    LOGD("start, loadDexFromMemoryDalvik()");
    JNIEnv *env = gDexFucker->env;
    // update mCookie
    LOGD("0");
    jobject oClassLoader = getClassLoader(env);
    jclass cClassLoader = (*env).GetObjectClass(oClassLoader);
    jclass cBaseDexClassLoader = (*env).GetSuperclass(cClassLoader);
    jfieldID fPathList = (*env).GetFieldID(cBaseDexClassLoader, "pathList",
                                           "Ldalvik/system/DexPathList;");
    jobject oPathList = (*env).GetObjectField(oClassLoader, fPathList);
    LOGD("1");
    jclass cDexPathList = (*env).GetObjectClass(oPathList);
    jfieldID fDexElements = (*env).GetFieldID(cDexPathList, "dexElements",
                                              "[Ldalvik/system/DexPathList$Element;");
    auto oDexElements =
            reinterpret_cast<jobjectArray>((*env).GetObjectField(oPathList, fDexElements));
    LOGD("2");
    hookDvmRawDexFileOpen();
    LOGD("3");
    jclass cDexFile = (*env).FindClass("dalvik/system/DexFile");
    jmethodID mLoadDex =
            (*env).GetStaticMethodID(cDexFile, "loadDex",
                                     "(Ljava/lang/String;Ljava/lang/String;I)Ldalvik/system/DexFile;");
    jstring fakeClassesDexName = (*env).NewStringUTF(gDexFucker->fakeClassesDexName.data());
    jstring odexDir = (*env).NewStringUTF(getOdexDir(env).data());
    jobject oDexFile = (*env).CallStaticObjectMethod(cDexFile, mLoadDex, fakeClassesDexName,
                                                     odexDir, 0);
    LOGD("3.5");
    jfieldID fMCookie = (*env).GetFieldID(cDexFile, "mCookie", "I");
    LOGD("new cookie: 0x%x", (*env).GetIntField(oDexFile, fMCookie));
    jclass cElement = (*env).FindClass("dalvik/system/DexPathList$Element");
    jmethodID mElement = (*env).GetMethodID(cElement, "<init>",
                                            "(Ljava/io/File;ZLjava/io/File;Ldalvik/system/DexFile;)V");
    jobject oElement = (*env).NewObject(cElement, mElement, nullptr, JNI_FALSE, nullptr, oDexFile);
    LOGD("5");
    jsize oldLenDexElements = (*env).GetArrayLength(oDexElements);
    LOGD("oldLenDexElements: %d", oldLenDexElements);

    jobjectArray oDexElementsNew = (*env).NewObjectArray(oldLenDexElements + 1, cElement, nullptr);
    for (int i = 0; i < oldLenDexElements; i++) {
        auto t = (*env).GetObjectArrayElement(oDexElements, i);
        (*env).SetObjectArrayElement(oDexElementsNew, i, t);
    }
    (*env).SetObjectArrayElement(oDexElementsNew, oldLenDexElements, oElement);
    LOGD("6");

    (*env).SetObjectField(oPathList, fDexElements, oDexElementsNew);
    LOGD("finish, loadDexFromMemoryDalvik()");
}


RawDexFile *createRawDexFileFromMemoryDalvik() {
    LOGD("start, createRawDexFileFromMemoryDalvik()");
    /*
     * Set up the basic raw data pointers of a DexFile. This function isn't
     * meant for general use.
     */
    const u1 *data = (const u1 *) gDexFucker->dexBuf;
    DexHeader *pDexHeader = (DexHeader *) data;
    DexFile *pDexFile = new DexFile();
    pDexFile->baseAddr = data;
    pDexFile->pHeader = pDexHeader;
    pDexFile->pStringIds = (const DexStringId *) (data + pDexHeader->stringIdsOff);
    pDexFile->pTypeIds = (const DexTypeId *) (data + pDexHeader->typeIdsOff);
    pDexFile->pFieldIds = (const DexFieldId *) (data + pDexHeader->fieldIdsOff);
    pDexFile->pMethodIds = (const DexMethodId *) (data + pDexHeader->methodIdsOff);
    pDexFile->pProtoIds = (const DexProtoId *) (data + pDexHeader->protoIdsOff);
    pDexFile->pClassDefs = (const DexClassDef *) (data + pDexHeader->classDefsOff);
    pDexFile->pLinkData = (const DexLink *) (data + pDexHeader->linkOff);

    // check dex file size
    if (pDexHeader->fileSize != gDexFucker->dexLen) {
        LOGE("ERROR: stored file size (%d) != expected (%d)",
             pDexHeader->fileSize, gDexFucker->dexLen);
    }
    LOGD("init pDexFile, finish...");

    /*
     * Create auxillary data structures.
     *
     * We need a 4-byte pointer for every reference to a class, method, field,
     * or string constant.  Summed up over all loaded DEX files (including the
     * whoppers in the boostrap class path), this adds up to be quite a bit
     * of native memory.
     *
     * For more traditional VMs these values could be stuffed into the loaded
     * class file constant pool area, but we don't have that luxury since our
     * classes are memory-mapped read-only.
     *
     * The DEX optimizer will remove the need for some of these (e.g. we won't
     * use the entry for virtual methods that are only called through
     * invoke-virtual-quick), creating the possibility of some space reduction
     * at dexopt time.
     */
    size_t stringSize = pDexHeader->stringIdsSize * sizeof(struct StringObject *);
    size_t classSize = pDexHeader->typeIdsSize * sizeof(struct ClassObject *);
    size_t methodSize = pDexHeader->methodIdsSize * sizeof(struct Method *);
    size_t fieldSize = pDexHeader->fieldIdsSize * sizeof(struct Field *);
    size_t totalSize = sizeof(DvmDex) + stringSize + classSize + methodSize + fieldSize;
    u1 *blob = new u1[totalSize]();
    DvmDex *pDvmDex = (DvmDex *) blob;
    blob += sizeof(DvmDex);

    pDvmDex->pDexFile = pDexFile;
    pDvmDex->pHeader = pDexHeader;

    pDvmDex->pResStrings = (struct StringObject **) blob;
    blob += stringSize;
    pDvmDex->pResClasses = (struct ClassObject **) blob;
    blob += classSize;
    pDvmDex->pResMethods = (struct Method **) blob;
    blob += methodSize;
    pDvmDex->pResFields = (struct Field **) blob;
    LOGD("+++ DEX %p: allocateAux (%d+%d+%d+%d)*4 = %d bytes",
         pDvmDex, stringSize / 4, classSize / 4, methodSize / 4, fieldSize / 4,
         stringSize + classSize + methodSize + fieldSize);

    auto *newCache = new AtomicCache();
    newCache->numEntries = DEX_INTERFACE_CACHE_SIZE;
    newCache->entryAlloc = new u1[sizeof(AtomicCacheEntry) * newCache->numEntries +
                                  CPU_CACHE_WIDTH];
    /*
     * Adjust storage to align on a 32-byte boundary.  Each entry is 16 bytes
     * wide.  This ensures that each cache entry sits on a single CPU cache
     * line.
     */
    assert(sizeof(AtomicCacheEntry) == 16);
    newCache->entries = (AtomicCacheEntry *)
            (((int) newCache->entryAlloc + CPU_CACHE_WIDTH_1) & ~CPU_CACHE_WIDTH_1);
    pDvmDex->pInterfaceCache = newCache;
    pthread_mutex_init(&pDvmDex->modLock, nullptr);

    // dvmDex memMapping
    pDvmDex->memMap.baseAddr = pDvmDex->memMap.addr = (void *) data;
    pDvmDex->memMap.baseLength = pDvmDex->memMap.length = pDexHeader->fileSize;
    pDvmDex->isMappedReadOnly = false;
    LOGD("init pDvmDex, finish...");

    /*
     * Create the class lookup table.  This will eventually be appended
     * to the end of the .odex.
     *
     * We create a temporary link from the DexFile for the benefit of
     * class loading, below.
     */
    DexClassLookup *pClassLookup = nullptr;
    int numEntries = dexRoundUpPower2(pDexFile->pHeader->classDefsSize * 2);
    LOGD("numEntries: %d", numEntries);
    int allocSize = offsetof(DexClassLookup, table) + numEntries * sizeof(pClassLookup->table[0]);
    LOGD("allocSize: %d, table sizeof: %d", allocSize, sizeof(pClassLookup->table[0]));
    pClassLookup = (DexClassLookup *) new u1[allocSize]();
    LOGD("pClassLookup: %p", pClassLookup);
    pClassLookup->size = allocSize;
    pClassLookup->numEntries = numEntries;
    int maxProbes = 0;
    int totalProbes = 0;
    for (int i = 0; i < pDexFile->pHeader->classDefsSize; i++) {
        const DexClassDef *pClassDef = dexGetClassDef(pDexFile, (u4) i);
        const char *classDescriptor = dexStringByTypeIdx(pDexFile, pClassDef->classIdx);
        u4 hash = classDescriptorHash(classDescriptor);
        int mask = pClassLookup->numEntries - 1;
        int idx = hash & mask;
        /*
         * Find the first empty slot.  We oversized the table, so this is
         * guaranteed to finish.
         */
        int numProbes = 0;
        while (pClassLookup->table[idx].classDescriptorOffset != 0) {
            idx = (idx + 1) & mask;
            numProbes++;
        }
        pClassLookup->table[idx].classDescriptorHash = hash;
        pClassLookup->table[idx].classDescriptorOffset =
                static_cast<int>((u1 *) classDescriptor - pDexFile->baseAddr);
        pClassLookup->table[idx].classDefOffset =
                static_cast<int>((u1 *) pClassDef - pDexFile->baseAddr);
        if (numProbes > maxProbes) {
            maxProbes = numProbes;
        }
        totalProbes += numProbes;
    }
    LOGD("Class lookup: classes=%d slots=%d (%d%% occ) alloc=%d total=%d max=%d",
         pDexFile->pHeader->classDefsSize, numEntries,
         (100 * pDexFile->pHeader->classDefsSize) / numEntries,
         allocSize, totalProbes, maxProbes);
    LOGD("init pClassLookup, finish...");

    pDvmDex->pDexFile->pClassLookup = pClassLookup;

    RawDexFile *pRawDexFile = new RawDexFile();
    pRawDexFile->pDvmDex = pDvmDex;
    LOGD("finish, createRawDexFileFromMemoryDalvik()");
    return pRawDexFile;
}


void initDexFucker(DexFucker **ppDexFucker, const ConfigFileProxy *pConfigFileProxy) {
    LOGD("start, initDexFucker(DexFucker **ppDexFucker, const ConfigFileProxy *pConfigFileProxy)");
    DexFucker *dexFucker = new DexFucker();
    dexFucker->env = pConfigFileProxy->env;
    dexFucker->fakeClassesDexName = pConfigFileProxy->fakeClassesDexName;
    dexFucker->dexBuf = const_cast<char *>(getClassesDexBuf());
    dexFucker->dexLen = pConfigFileProxy->srcClassesDexSize;
    dexFucker->codeItemBuf = getCodeItemBuf();
    dexFucker->codeItemLen = pConfigFileProxy->srcCodeItemSize;

    const char *cur = dexFucker->codeItemBuf;
    size_t size = *(size_t *) cur;
    cur += 4;
    for (int i = 0; i < size; i++) {
        size_t keySize = *(size_t *) cur;
        cur += 4;
        string key = cur;
        cur += keySize;

        size_t valueSize = *(size_t *) cur;
        cur += 4;
        size_t codeBufOff = 0;

        auto *pCodeItemData = (CodeItemData *) (cur + codeBufOff);
        codeBufOff += 0x20; // data: 0x10B, two pointers: 0x10B (64 bit operating system)
        pCodeItemData->insns = (u2 *) (cur + codeBufOff);
        codeBufOff += pCodeItemData->insnsSize * sizeof(u2);
        if (pCodeItemData->triesSize > 0 && (pCodeItemData->insnsSize & 0x01)) {
            codeBufOff += 2;
        }
        if (pCodeItemData->triesSize > 0) {
            pCodeItemData->triesAndHandlersBuf = (u1 *) (cur + codeBufOff);
        } else {
            pCodeItemData->triesAndHandlersBuf = nullptr;
        }
        cur += valueSize;
        dexFucker->codeItem[key] = pCodeItemData;
    }
    LOGD("code item size: %d, map size: %d", size, dexFucker->codeItem.size());
    *ppDexFucker = dexFucker;
    LOGD("finish, initDexFucker(DexFucker **ppDexFucker, const ConfigFileProxy *pConfigFileProxy)");
}

const CodeItemData *getCodeItemByKey(const string &key) {
    auto it = gDexFucker->codeItem.find(key);
    if (it != gDexFucker->codeItem.end()) {
        return it->second;
    }
    LOGE("can't found code item data by key: %s", key.data());
    return nullptr;
}