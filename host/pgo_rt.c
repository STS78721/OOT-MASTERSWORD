/* (hookshot) Profile-guided builds (RX_PGO=gen, nx2/build.py): what clang's -fprofile-instr-generate needs, in place of compiler-rt's
   profile library, which this toolchain lacks. The counters sit in sections .lprfc/.lprfd/.lprfn; this writes them in LLVM's raw
   format for llvm-profdata.
   HOS_PGO=<path>: every 500 ms, if <path>.reset exists the counters are cleared, and if <path>.dump exists they are written to <path>;
   each file is deleted when done. So a run can leave out the loading. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int __llvm_profile_runtime = 0;

#pragma section(".lprfc$A", read, write)
#pragma section(".lprfc$Z", read, write)
#pragma section(".lprfd$A", read, write)
#pragma section(".lprfd$Z", read, write)
#pragma section(".lprfn$A", read)
#pragma section(".lprfn$Z", read)
__declspec(allocate(".lprfc$A")) __attribute__((used, aligned(8))) char pgo_cnt_a[8] = {0};
__declspec(allocate(".lprfc$Z")) __attribute__((used, aligned(8))) char pgo_cnt_z[8] = {0};
__declspec(allocate(".lprfd$A")) __attribute__((used, aligned(8))) char pgo_dat_a[72] = {0};
__declspec(allocate(".lprfd$Z")) __attribute__((used, aligned(8))) char pgo_dat_z[72] = {0};
__declspec(allocate(".lprfn$A")) __attribute__((used)) const char pgo_nam_a[1] = {0};
__declspec(allocate(".lprfn$Z")) __attribute__((used)) const char pgo_nam_z[1] = {0};

enum { DATA_SIZE = 72 };

static SRWLOCK pgo_lock = SRWLOCK_INIT;
static char pgo_path[MAX_PATH];
static volatile LONG pgo_stopping;

static void pgo_write(const char *path) {
    char tmp[MAX_PATH + 16];
    const char *cb = pgo_cnt_a + 8, *ce = pgo_cnt_z, *db = pgo_dat_a + DATA_SIZE, *de = pgo_dat_z, *nb = pgo_nam_a + 1, *ne = pgo_nam_z;
    const uint64_t ndata = (uint64_t)(de - db) / DATA_SIZE, ncnt = (uint64_t)(ce - cb) / 8, nnames = (uint64_t)(ne - nb);
    const uint64_t header[] = {
        0xff6c70726f667281ULL,        /* magic: \xfflprofr\x81 */
        11,                           /* (the format llvm-profdata 24 reads) */
        0,                            /* binary ids */
        ndata, 0, ncnt, 0,
        0, 0,                         /* bitmap */
        0, 0, 0,                      /* uniform counters */
        nnames,
        (uint64_t)(uintptr_t)cb - (uint64_t)(uintptr_t)db,   /* CountersDelta */
        (uint64_t)0,                  /* BitmapDelta */
        (uint64_t)(uintptr_t)nb,      /* NamesDelta */
        0, 0,                         /* vtables */
        2,                            /* ValueKindLast (IPVK_Last) */
    };
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    AcquireSRWLockExclusive(&pgo_lock);
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        ReleaseSRWLockExclusive(&pgo_lock);
        return;
    }
    fwrite(header, sizeof header, 1, f);
    /* (the records' pointers to their counters are 32 bits in a COFF image, the rest zero, and the counters come first: negative) */
    char *copy = malloc((size_t)(de - db));
    if (copy) {
        memcpy(copy, db, (size_t)(de - db));
        for (uint64_t i = 0; i < ndata; i++) {
            int32_t rel;
            memcpy(&rel, copy + i * DATA_SIZE + 16, 4);
            const int64_t wide = rel;
            memcpy(copy + i * DATA_SIZE + 16, &wide, 8);
        }
        fwrite(copy, 1, (size_t)(de - db), f);
        free(copy);
    }
    fwrite(cb, 1, (size_t)(ce - cb), f);
    fwrite(nb, 1, (size_t)nnames, f);
    static const char zero[8];
    fwrite(zero, 1, (size_t)(-(int64_t)nnames & 7), f);
    fclose(f);
    MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING);
    ReleaseSRWLockExclusive(&pgo_lock);
}

static DWORD WINAPI pgo_thread(void *arg) {
    const char *path = arg;
    char dump[MAX_PATH + 16], reset[MAX_PATH + 16];
    int ticks = 0;
    snprintf(dump, sizeof dump, "%s.dump", path);
    snprintf(reset, sizeof reset, "%s.reset", path);
    while (!InterlockedCompareExchange(&pgo_stopping, 0, 0)) {
        Sleep(500);
        if (InterlockedCompareExchange(&pgo_stopping, 0, 0)) break;
        if (GetFileAttributesA(reset) != INVALID_FILE_ATTRIBUTES) {
            memset(pgo_cnt_a + 8, 0, (size_t)(pgo_cnt_z - (pgo_cnt_a + 8)));
            DeleteFileA(reset);
            ticks = 0;
        }
        if (GetFileAttributesA(dump) != INVALID_FILE_ATTRIBUTES) {
            pgo_write(path);
            DeleteFileA(dump);
            ticks = 0;
            continue;
        }
        if (++ticks >= 20) {   /* periodic dump every 10 s so live play counters survive hard exits */
            pgo_write(path);
            ticks = 0;
        }
    }
    return 0;
}

/* (MasterSword) the Vulkan back end's own copy (nx2/build.py, RX_VK_PGO=gen) reads another variable: -DPGO_ENV="RX_VK_PGO_FILE" */
#ifndef PGO_ENV
#define PGO_ENV "HOS_PGO"
#endif
__attribute__((constructor)) static void pgo_start(void) {
    const char *e = getenv(PGO_ENV);
    if (e && *e) {
        snprintf(pgo_path, sizeof pgo_path, "%s", e);
    } else {
        DWORD n = GetModuleFileNameA(NULL, pgo_path, MAX_PATH);
        if (!n || n >= MAX_PATH) {
            pgo_path[0] = 0;
            return;
        }
        while (n && pgo_path[n - 1] != '\\' && pgo_path[n - 1] != '/') n--;
        if (n + sizeof("vk_pgo_capture.profraw") > sizeof pgo_path) {
            pgo_path[0] = 0;
            return;
        }
        memcpy(pgo_path + n, "vk_pgo_capture.profraw", sizeof("vk_pgo_capture.profraw"));
    }
    CreateThread(NULL, 0, pgo_thread, pgo_path, 0, NULL);
}

__attribute__((destructor)) static void pgo_stop(void) {
    InterlockedExchange(&pgo_stopping, 1);
    if (pgo_path[0]) pgo_write(pgo_path);
}
