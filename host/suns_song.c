/* (MasterSword Sun's Song, 2026-11-15) Who woke whom: a timeline of the game's own waits and wake-ups, for the frame's
   critical path where the sampler (Mask of Truth) only sees that nothing runs. Mask of Truth in Town: ~30% of a frame with
   no game, renderer or driver thread running, the main thread waiting on a job event and the job workers waiting for
   jobs nobody had posted yet; the GPU-bound wait 0 ms, no vsync quantization, the sleeps on time. What posts the jobs late
   is a question of order, which a sample cannot answer and this records exactly.

   HOS_WAKE_TRACE=<file> (diagnostic; nothing is recorded without it): while <file>.go exists, every guest event wait
   (begin, end), event signal, message queue send and receive (begin, end) and sleep (begin, end), with the time, the
   thread, the object and the guest caller, goes into a buffer (4M records, ~130 MB once used); written to <file> as CSV
   when the .go file goes away. scripts/suns_song.py reads it.

   Also hos_name_interned: a thread's name in a pool that is never freed. hos_thread_name points into the thread, which is
   freed when it ends, and the HOS_WAKE_LAT / HOS_WAIT_LOG / HOS_WAIT_WHO tables kept that pointer: printing one whose
   thread had ended read freed memory (a crash in strnlen on the bench, 2026-11-15). */
#include "hos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

const char *hos_name_interned(const char *n) {
    enum { POOL = 1024 };
    static SRWLOCK lk = SRWLOCK_INIT;
    static char *names[POOL];
    static int count;
    if (!n) return "?";
    AcquireSRWLockShared(&lk);
    for (int i = 0; i < count; i++)
        if (!strcmp(names[i], n)) {
            const char *r = names[i];
            ReleaseSRWLockShared(&lk);
            return r;
        }
    ReleaseSRWLockShared(&lk);
    AcquireSRWLockExclusive(&lk);
    const char *r = NULL;
    for (int i = 0; i < count && !r; i++)
        if (!strcmp(names[i], n)) r = names[i];
    if (!r) {
        if (count < POOL) {
            names[count] = _strdup(n);
            r = names[count] ? names[count++] : "?";
        } else {
            r = "(more threads than the name pool)";
        }
    }
    ReleaseSRWLockExclusive(&lk);
    return r;
}

typedef struct {
    int64_t t;
    uint64_t key, lr, thread;   /* (thread: the HosThread's address, 0 for a host thread such as the vsync's) */
    const char *name;
    int kind;
} WakeRec;
enum { WAKE_CAP = 1 << 22 };
static const char *const kind_names[] = {"?", "wait", "woke", "signal", "send", "recv", "got", "sleep", "slept"};
static WakeRec *wake_buf;
static volatile LONG64 wake_n;
static volatile LONG wake_on;
static const char *wake_path;

static DWORD WINAPI wake_main(void *p) {
    (void)p;
    char go[MAX_PATH];
    snprintf(go, sizeof go, "%s.go", wake_path);
    for (;;) {
        Sleep(200);
        const int want = GetFileAttributesA(go) != INVALID_FILE_ATTRIBUTES;
        if (want && !wake_on) {
            if (!wake_buf) wake_buf = (WakeRec *)VirtualAlloc(NULL, sizeof(WakeRec) * (size_t)WAKE_CAP, MEM_RESERVE | MEM_COMMIT,
                                                             PAGE_READWRITE);
            if (!wake_buf) {
                hos_log("sun's song: no memory for the trace");
                continue;
            }
            InterlockedExchange64(&wake_n, 0);
            InterlockedExchange(&wake_on, 1);
            hos_log("sun's song: recording who woke whom into %s", wake_path);
        } else if (!want && wake_on) {
            InterlockedExchange(&wake_on, 0);
            Sleep(100);   /* (notes already past their check finish) */
            const int64_t n = wake_n < WAKE_CAP ? wake_n : WAKE_CAP;
            FILE *f = fopen(wake_path, "w");
            if (!f) {
                hos_log("sun's song: could not write %s", wake_path);
                continue;
            }
            static char buffer[1 << 20];
            setvbuf(f, buffer, _IOFBF, sizeof buffer);
            fputs("t_ns,thread,name,kind,key,lr\n", f);
            for (int64_t i = 0; i < n; i++) {
                const WakeRec *r = &wake_buf[i];
                if (!r->kind) continue;   /* (a slot taken but never filled) */
                fprintf(f, "%lld,%llx,%s,%s,%llx,%llx\n", (long long)r->t, (unsigned long long)r->thread, r->name ? r->name : "host",
                        kind_names[r->kind], (unsigned long long)r->key, (unsigned long long)r->lr);
            }
            fclose(f);
            hos_log("sun's song: %lld records written to %s%s", (long long)n, wake_path,
                    wake_n > WAKE_CAP ? " (the buffer filled: the rest were dropped)" : "");
        }
    }
    return 0;
}

/* kind: 1 wait, 2 woke, 3 signal, 4 send, 5 recv, 6 got, 7 sleep, 8 slept */
void hos_wake_note(int kind, uint64_t key, uint64_t lr) {
    static int on = -1;
    if (on < 0) {
        static SRWLOCK lk = SRWLOCK_INIT;
        AcquireSRWLockExclusive(&lk);
        if (on < 0) {
            wake_path = getenv("HOS_WAKE_TRACE");
            on = wake_path && *wake_path;
            if (on) CreateThread(NULL, 0, wake_main, NULL, 0, NULL);
        }
        ReleaseSRWLockExclusive(&lk);
    }
    if (!on || !wake_on) return;
    const LONG64 i = InterlockedIncrement64(&wake_n) - 1;
    if (i >= WAKE_CAP) return;
    WakeRec *r = &wake_buf[i];
    HosThread *self = hos_self();
    static _Thread_local HosThread *named_for;   /* (the thread's interned name, looked up once a thread) */
    static _Thread_local const char *name;
    if (self && named_for != self) named_for = self, name = hos_name_interned(hos_thread_name(self));
    r->t = hos_now_ns();
    r->key = key, r->lr = lr;
    r->thread = (uint64_t)(uintptr_t)self;
    r->name = self ? name : "host";
    r->kind = kind;
}
