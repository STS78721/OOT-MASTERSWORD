#include "hos.h"
#include <stdlib.h>
#include <string.h>
#ifndef HOOKSHOT_ENV_H
#include "../gpu/vk/compat/hookshot_env.h"
#endif

/* The SDK's memcpy, memmove, memset, memcmp, strlen and strcmp done by the host's: ENTRY_HOOK_SYM(_FROM)s, spliced at the entry of the
   SDK's own functions by the standalone lift (nx2/entry_hooks.py), which stay behind them. Each does what the SDK's does for
   every call it takes: memcpy is the SDK's memmove-safe one (overlapping copies come out as memmove's), memcpy, memmove and memset
   return the destination as they found it in x0, memcmp and strcmp the difference of the first unequal bytes (unsigned) as a
   32-bit int, zero-extended in x0 as the SDK's leaves it. What they write is marked for the GPU mirror's scan as the lifted
   stores mark it: every 4 KB page of the range, after the data (HOS_WRITTEN).
   HOS_GUEST_LIBC=1: every call is the game's own (lifted) code, for an A/B on one build. The first call logs which it is. */
/* (A copy or fill over LIBC_PIECE bytes goes in pieces below Windows' non-temporal threshold so destinations stay cached, each
   marked once written, with an interrupt poll (A64_POLL) between them. memset under 8 bytes and memcmp under 16 stay in the
   lifted SDK code: ENTRY_HOOK_SYM_FROM's size check is tested in the spliced line itself.) */
#define LIBC_PIECE (256u << 10)
static int guest_libc(void) {
    static int v = -1;
    int on = __atomic_load_n(&v, __ATOMIC_RELAXED);
    if (__builtin_expect(on < 0, 0)) {
        static int said;
        on = env_on("HOS_GUEST_LIBC");
        if (!__atomic_exchange_n(&said, 1, __ATOMIC_RELAXED))
            hos_log(on ? "libc: HOS_GUEST_LIBC=1: memcpy, memmove, memset, memcmp, strlen and strcmp are the game's own"
                       : "libc: memcpy, memmove, memset, memcmp, strlen and strcmp are the host's (HOS_GUEST_LIBC=1: the game's own)");
        __atomic_store_n(&v, on, __ATOMIC_RELAXED);
    }
    return on;
}

#if defined(RX_WEB_MT) && RX_WEB_MT
void rx_wait_writebacks_reading(const void *p, uint64_t n);   /* gpu/render/rx_nvn.h */
#define WAIT_WRITEBACKS(p, n) rx_wait_writebacks_reading(p, n)
#else
#define WAIT_WRITEBACKS(p, n) ((void)0)
#endif

/* d <- s, n bytes over LIBC_PIECE, memmove's result, in pieces: each marked once written, an interrupt poll between them */
static __attribute__((noinline)) void copy_pieces(Ctx *c, uint8_t *d, const uint8_t *s, uint64_t n) {
    if (d <= s || d >= s + n) {          /* (forward: a piece overwrites only source bytes already copied) */
        for (uint64_t at = 0; at < n;) {
            const uint64_t k = n - at < LIBC_PIECE ? n - at : LIBC_PIECE;
            memmove(d + at, s + at, (size_t)k);
            HOS_WRITTEN(d + at, (size_t)k);
            if ((at += k) < n) A64_POLL(c);
        }
    } else {                             /* (the destination above an overlapping source: from the end) */
        for (uint64_t end = n; end;) {
            const uint64_t k = end < LIBC_PIECE ? end : LIBC_PIECE;
            memmove(d + end - k, s + end - k, (size_t)k);
            HOS_WRITTEN(d + end - k, (size_t)k);
            if ((end -= k)) A64_POLL(c);
        }
    }
}
/* d <- s, n bytes, memmove's result, marked after it is written */
static inline void copy_marked(Ctx *c, uint8_t *d, const uint8_t *s, uint64_t n) {
    if (__builtin_expect(n > LIBC_PIECE, 0)) {
        copy_pieces(c, d, s, n);
        return;
    }
    memmove(d, s, (size_t)n);
    HOS_WRITTEN(d, (size_t)n);
}
/* the same for memset's fill */
static __attribute__((noinline)) void fill_pieces(Ctx *c, uint8_t *d, int v, uint64_t n) {
    for (uint64_t at = 0; at < n;) {
        const uint64_t k = n - at < LIBC_PIECE ? n - at : LIBC_PIECE;
        memset(d + at, v, (size_t)k);
        HOS_WRITTEN(d + at, (size_t)k);
        if ((at += k) < n) A64_POLL(c);
    }
}

ENTRY_HOOK_SYM(memcpy) {
    if (guest_libc()) return 0;
    const uint64_t n = ARG(2);
    if (!hos_guest_range(ARG(0), n) || !hos_guest_range(ARG(1), n)) return 0;   /* (past the guest window, the game's own copy) */
    if (n) {
        WAIT_WRITEBACKS(GP(ARG(1)), n);   /* (TOTK copies some rendered maps with the CPU) */
        copy_marked(c, (uint8_t *)GP(ARG(0)), (const uint8_t *)GP(ARG(1)), n);
    }
    return 1;
}

ENTRY_HOOK_SYM(memmove) {
    if (guest_libc()) return 0;
    const uint64_t n = ARG(2);
    if (!hos_guest_range(ARG(0), n) || !hos_guest_range(ARG(1), n)) return 0;   /* (past the guest window, the game's own copy) */
    if (n) {
        WAIT_WRITEBACKS(GP(ARG(1)), n);
        copy_marked(c, (uint8_t *)GP(ARG(0)), (const uint8_t *)GP(ARG(1)), n);
    }
    return 1;
}

ENTRY_HOOK_SYM_FROM(memset, 8) {
    if (guest_libc()) return 0;
    const uint64_t n = ARG(2);
    if (n < 8 || !hos_guest_range(ARG(0), n)) return 0;   /* (past the guest window, the game's own; under 8 bytes, too) */
    uint8_t *d = (uint8_t *)GP(ARG(0));
    if (__builtin_expect(n > LIBC_PIECE, 0)) fill_pieces(c, d, (int)(uint8_t)ARG(1), n);
    else {
        memset(d, (int)(uint8_t)ARG(1), (size_t)n);
        HOS_WRITTEN(d, (size_t)n);
    }
    return 1;
}

ENTRY_HOOK_SYM_FROM(memcmp, 16) {
    if (guest_libc() || ARG(2) < 16) return 0;
    const uint8_t *a = (const uint8_t *)GP(ARG(0)), *b = (const uint8_t *)GP(ARG(1));
    const size_t n = (size_t)ARG(2);
    int r = 0;
    if (memcmp(a, b, n)) {
        size_t i = 0;
        while (a[i] == b[i]) i++;
        r = (int)a[i] - (int)b[i];
    }
    RET((uint32_t)r);   /* (w0: the SDK's leaves x0's upper half 0) */
    return 1;
}

ENTRY_HOOK_SYM(strlen) {
    if (guest_libc()) return 0;
    RET(strlen((const char *)GP(ARG(0))));
    return 1;
}

ENTRY_HOOK_SYM(strcmp) {
    if (guest_libc()) return 0;
    const uint8_t *a = (const uint8_t *)GP(ARG(0)), *b = (const uint8_t *)GP(ARG(1));
    int r = 0;
    if (strcmp((const char *)a, (const char *)b)) {
        size_t i = 0;
        while (a[i] && a[i] == b[i]) i++;
        r = (int)a[i] - (int)b[i];
    }
    RET((uint32_t)r);   /* (w0: the SDK's leaves x0's upper half 0) */
    return 1;
}
