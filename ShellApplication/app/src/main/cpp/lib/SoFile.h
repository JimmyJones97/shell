//
// Created by ChenD on 2019/9/9.
//

#ifndef SHELLAPPLICATION_SOFILE_H
#define SHELLAPPLICATION_SOFILE_H

#include <link.h>
#include <string>

using namespace std;

#define SOINFO_NAME_LEN 128

typedef void (*linker_function_t)();

// Returns the address of the page containing address 'x'.
#define PAGE_START(x)  ((x) & PAGE_MASK)

// Returns the offset of address 'x' in its page.
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)

// Returns the address of the next page after address 'x', unless 'x' is
// itself at the start of a page.
#define PAGE_END(x)    PAGE_START((x) + (PAGE_SIZE-1))

#define MAYBE_MAP_FLAG(x, from, to)    (((x) & (from)) ? (to) : 0)
#define PFLAGS_TO_PROT(x)            (MAYBE_MAP_FLAG((x), PF_X, PROT_EXEC) | \
                                      MAYBE_MAP_FLAG((x), PF_R, PROT_READ) | \
                                      MAYBE_MAP_FLAG((x), PF_W, PROT_WRITE))

struct SoInfo {
    char name[SOINFO_NAME_LEN];
    const ElfW(Phdr) *phdr;
    size_t phnum;
    ElfW(Addr) entry;
    ElfW(Addr) base;
    unsigned size;

    uint32_t unused1;  // DO NOT USE, maintained for compatibility.

    ElfW(Dyn) *dynamic;

    uint32_t unused2; // DO NOT USE, maintained for compatibility
    uint32_t unused3; // DO NOT USE, maintained for compatibility

    SoInfo *next;
    unsigned flags;

    const char *strtab;
    ElfW(Sym) *symtab;

    size_t nbucket;
    size_t nchain;
    unsigned *bucket;
    unsigned *chain;

    unsigned *plt_got;

    ElfW(Rel) *plt_rel;
    size_t plt_rel_count;

    ElfW(Rel) *rel;
    size_t rel_count;

    linker_function_t *preinit_array;
    size_t preinit_array_count;

    linker_function_t *init_array;
    size_t init_array_count;
    linker_function_t *fini_array;
    size_t fini_array_count;

    linker_function_t init_func;
    linker_function_t fini_func;

    // ARM EABI section used for stack unwinding.
    unsigned *ARM_exidx;
    size_t ARM_exidx_count;

    size_t ref_count;
    link_map link_map;

    bool constructors_called;

    // When you read a virtual address from the ELF file, add this
    // value to get the corresponding address in the process' address space.
    ElfW(Addr) load_bias;

    bool has_text_relocations;
    bool has_DT_SYMBOLIC;
};

void openElf(const char *soBuf, const string &soFileName, SoInfo *soInfo);

void reserveAddressSpace(const ElfW(Phdr) *phdr, const size_t phdrNum, ElfW(Addr) *retLoadSize,
                         void **retLoadStart, ElfW(Addr) *retLoadBias);

ElfW(Addr) phdrTableGetLoadSize(const ElfW(Phdr) *phdrTable, const size_t phdrCount,
                                ElfW(Addr) *retOutMinVirAddr, ElfW(Addr) *retOutMaxVirAddr);

void loadSegments(const ElfW(Phdr) *phdrTable, const size_t phdrCount, const ElfW(Addr) loadBias,
                  const char *soBuf);

void findPhdr(const ElfW(Phdr) *phdrTable, const size_t phdrCount, const ElfW(Addr) loadBias,
              ElfW(Phdr) **loadedPhdr);

bool checkPhdr(ElfW(Addr) loaded, const ElfW(Phdr) *phdrTable, const size_t phdrCount,
               const ElfW(Addr) loadBias, ElfW(Phdr) **loadedPhdr);

#endif //SHELLAPPLICATION_SOFILE_H
