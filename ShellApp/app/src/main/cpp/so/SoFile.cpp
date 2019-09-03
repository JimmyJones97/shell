//
// Created by dalunlun on 2019/8/30.
//

#include <sys/mman.h>
#include "SoFile.h"
#include "../common/Util.h"
#include <errno.h>

void openElf(const char *soBuf, const string &soFileName, SoInfo *soInfo) {
    // load library
    LOGD("loading library...");
    ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *) soBuf;
    ElfW(Phdr) *phdr = (ElfW(Phdr) *) (soBuf + ehdr->e_phoff);

    ElfW(Addr) loadSize = 0;
    void *loadStart = nullptr;
    ElfW(Addr) loadBias = 0;
    ElfW(Phdr) *loadedPhdr = nullptr;
    reserveAddressSpace(phdr, ehdr->e_phnum, &loadSize, &loadStart, &loadBias);
    loadSegments(phdr, ehdr->e_phnum, loadBias, soBuf);
    findPhdr(phdr, ehdr->e_phnum, loadBias, &loadedPhdr);

    strlcpy(soInfo->name, soFileName.data(), sizeof(soInfo->name));
    soInfo->base = reinterpret_cast<ElfW(Addr)>(loadStart);
    soInfo->size = loadSize;
    soInfo->load_bias = loadBias;
    soInfo->flags = 0;
    soInfo->entry = 0;
    soInfo->dynamic = nullptr;
    soInfo->phnum = ehdr->e_phnum;
    soInfo->phdr = loadedPhdr;
}

void reserveAddressSpace(const ElfW(Phdr) *phdr, const size_t phdrNum, ElfW(Addr) *retLoadSize,
                         void **retLoadStart, ElfW(Addr) *retLoadBias) {
    LOGD("reserveAddressSpace, start...");
    ElfW(Addr) minVirAddr;
    *retLoadSize = phdrTableGetLoadSize(phdr, phdrNum, &minVirAddr, nullptr);
    LOGD("loadSize: %x", *retLoadSize);
    LOGD("minVirAddr: %x", minVirAddr);
    assert(*retLoadSize != 0);

    uint8_t *addr = reinterpret_cast<uint8_t *>(minVirAddr);
    *retLoadStart = mmap(addr, *retLoadSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    LOGD("load start: %p", *retLoadStart);
    assert(*retLoadStart != MAP_FAILED);

    *retLoadBias = (uint8_t *) (*retLoadStart) - addr;
    LOGD("load bias: %x", *retLoadBias);
    LOGD("reserveAddressSpace, finish...");
}

ElfW(Addr) phdrTableGetLoadSize(const ElfW(Phdr) *phdrTable, const size_t phdrCount,
                                ElfW(Addr) *retOutMinVirAddr, ElfW(Addr) *retOutMaxVirAddr) {
    LOGD("phdrTableGetLoadSize, start...");
    ElfW(Addr) min_vaddr = 0xFFFFFFFFU;
    ElfW(Addr) max_vaddr = 0x00000000U;

    bool found_pt_load = false;
    for (size_t i = 0; i < phdrCount; ++i) {
        const ElfW(Phdr) *phdr = &phdrTable[i];

        if (phdr->p_type != PT_LOAD) {
            continue;
        }
        found_pt_load = true;

        if (phdr->p_vaddr < min_vaddr) {
            min_vaddr = phdr->p_vaddr;
        }

        if (phdr->p_vaddr + phdr->p_memsz > max_vaddr) {
            max_vaddr = phdr->p_vaddr + phdr->p_memsz;
        }
    }
    if (!found_pt_load) {
        min_vaddr = 0x00000000U;
    }

    min_vaddr = PAGE_START(min_vaddr);
    max_vaddr = PAGE_END(max_vaddr);

    if (retOutMinVirAddr != nullptr) {
        *retOutMinVirAddr = min_vaddr;
    }
    if (retOutMaxVirAddr != nullptr) {
        *retOutMaxVirAddr = max_vaddr;
    }
    LOGD("phdrTableGetLoadSize, finish...");
    return max_vaddr - min_vaddr;
}

void loadSegments(const ElfW(Phdr) *phdrTable, const size_t phdrCount, const ElfW(Addr) loadBias,
                  const char *soBuf) {
    LOGD("loadSegments, start...");
    for (size_t i = 0; i < phdrCount; ++i) {
        LOGD("loading[ %d ]", i);
        const ElfW(Phdr) *phdr = &phdrTable[i];

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        // Segment addresses in memory.
        ElfW(Addr) seg_start = phdr->p_vaddr + loadBias;
        ElfW(Addr) seg_end = seg_start + phdr->p_memsz;

        ElfW(Addr) seg_page_start = PAGE_START(seg_start);
        ElfW(Addr) seg_page_end = PAGE_END(seg_end);

        ElfW(Addr) seg_file_end = seg_start + phdr->p_filesz;

        // File offsets.
        ElfW(Addr) file_start = phdr->p_offset;
        ElfW(Addr) file_end = file_start + phdr->p_filesz;

        ElfW(Addr) file_page_start = PAGE_START(file_start);
        ElfW(Addr) file_length = file_end - file_page_start;

        LOGD("memory mapping segment...");
        if (file_length != 0) {
            LOGD("mmap start: %p, len: %x", (void *) seg_page_start, file_length);
            void *seg_addr = mmap((void *) seg_page_start, file_length,
                                  PFLAGS_TO_PROT(phdr->p_flags) | PROT_WRITE,
                                  MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (seg_addr == MAP_FAILED) {
                LOGE("mmap error: %s", strerror(errno));
                assert(seg_addr != MAP_FAILED);
            }

            memcpy(seg_addr, soBuf + file_page_start, file_length);
            mprotect(seg_addr, file_length, PFLAGS_TO_PROT(phdr->p_flags));
        }
        LOGD("memory map segment, finish...");

        // if the segment is writable, and does not end on a page boundary,
        // zero-fill it until the page limit.
        if ((phdr->p_flags & PF_W) != 0 && PAGE_OFFSET(seg_file_end) > 0) {
            memset((void *) seg_file_end, 0, PAGE_SIZE - PAGE_OFFSET(seg_file_end));
        }

        seg_file_end = PAGE_END(seg_file_end);

        // seg_file_end is now the first page address after the file
        // content. If seg_end is larger, we need to zero anything
        // between them. This is done by using a private anonymous
        // map for all extra pages.
        if (seg_page_end > seg_file_end) {
            void *zeromap = mmap((void *) seg_file_end, seg_page_end - seg_file_end,
                                 PFLAGS_TO_PROT(phdr->p_flags),
                                 MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            assert(zeromap != MAP_FAILED);
        }
    }
    LOGD("loadSegments, finish...");
}

void findPhdr(const ElfW(Phdr) *phdrTable, const size_t phdrCount, const ElfW(Addr) loadBias,
              ElfW(Phdr) **loadedPhdr) {
    LOGD("findPhdr, start...");
    const ElfW(Phdr) *phdr_limit = phdrTable + phdrCount;

    // If there is a PT_PHDR, use it directly.
    bool checkFlag = false;
    for (const ElfW(Phdr) *phdr = phdrTable; phdr < phdr_limit; ++phdr) {
        if (phdr->p_type == PT_PHDR) {
            checkFlag = checkPhdr(loadBias + phdr->p_vaddr, phdrTable, phdrCount, loadBias,
                                  loadedPhdr);
        }
    }

    // Otherwise, check the first loadable segment. If its file offset
    // is 0, it starts with the ELF header, and we can trivially find the
    // loaded program header from it.
    for (const ElfW(Phdr) *phdr = phdrTable; phdr < phdr_limit; ++phdr) {
        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_offset == 0) {
                ElfW(Addr) elf_addr = loadBias + phdr->p_vaddr;
                const ElfW(Ehdr) *ehdr = (const ElfW(Ehdr) *) (void *) elf_addr;
                ElfW(Addr) offset = ehdr->e_phoff;
                checkFlag = checkPhdr((ElfW(Addr)) ehdr + offset, phdrTable, phdrCount, loadBias,
                                      loadedPhdr);
            }
            break;
        }
    }
    assert(checkFlag);
    LOGD("findPhdr, finish...");
}

bool checkPhdr(ElfW(Addr) loaded, const ElfW(Phdr) *phdrTable, const size_t phdrCount,
               const ElfW(Addr) loadBias, ElfW(Phdr) **loadedPhdr) {
    LOGD("checkPhdr, start...");
    const ElfW(Phdr) *phdr_limit = phdrTable + phdrCount;
    ElfW(Addr) loaded_end = loaded + (phdrCount * sizeof(ElfW(Phdr)));
    for (const ElfW(Phdr) *phdr = phdrTable; phdr < phdr_limit; ++phdr) {
        if (phdr->p_type != PT_LOAD) {
            continue;
        }
        ElfW(Addr) seg_start = phdr->p_vaddr + loadBias;
        ElfW(Addr) seg_end = phdr->p_filesz + seg_start;
        if (seg_start <= loaded && loaded_end <= seg_end) {
            *loadedPhdr = reinterpret_cast<ElfW(Phdr) *>(loaded);
            LOGD("checkPhdr, finish...");
            return true;
        }
    }
    LOGE("loaded phdr %x not in loadable segment", loaded);
    return false;
}
