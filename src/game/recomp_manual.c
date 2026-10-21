/* Hand-written replacements for individual lifted functions.
 * This is an ALLOWED place for fixes (see AGENTS.md §2).
 * Register each override in the manual lookup table so ICALLs find it.
 *
 * Pattern:
 *   void sub_001A3F50(void) {
 *       eax = 1;          // return success
 *       esp += 4; return; // clean up the fake return address
 *   }
 */

/* Game integration points required by recomp_types.h / kernel_bridge.c.
 * Mirrors the toolkit template templates/new-game/src/recomp_manual.c. */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>   /* ptrdiff_t */

#include "gen/recomp_types.h"   /* RECOMP_TLS must match the runtime definition */

/* ── ICALL trace ring buffer ───────────────────────────────── */

/* The runtime owns these (src/kernel/xbox_memory_layout.c); declare only. */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

extern ptrdiff_t g_xbox_mem_offset;

/* ── Manual function overrides ─────────────────────────────── */

/* Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table. */
recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

void recomp_icall_fail_log(uint32_t va)
{
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (total calls: %llu)\n",
            va, (unsigned long long)g_icall_count);

    /* Dump last 16 call targets from the ring buffer */
    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
}

/* An indirect call whose target is not code: a null or wild function pointer.
 * Skipping these is right; skipping them silently is not. Rate-limited per
 * address. */
void recomp_icall_not_code_log(uint32_t va)
{
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped %llu time(s) "
                    "(null or wild function pointer, at call #%llu)\n",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    fflush(stderr);
}

/* ── Untranslated instructions ─────────────────────────────────────────── */

#include <stdlib.h>

void recomp_unimpl(const char *text, uint32_t va)
{
    static int printed;
    const char *trap = getenv("RECOMP_UNIMPL_TRAP");
    int stop = trap && *trap && *trap != '0';

    if (printed < 50 || stop) {
        printed++;
        fprintf(stderr,
                "[UNIMPL] untranslated instruction REACHED: `%s` at 0x%08X"
                " (a no-op; set RECOMP_UNIMPL_TRAP=1 to stop here)\n",
                text, va);
        fflush(stderr);
    }
    if (stop) abort();
}

/* ── DVD media authentication ──────────────────────────────────────────── */

/* The ISO in game/ is a trimmed image: the video partition and the security
 * sector slot (0x182FF800) are zeroed, so no Microsoft-signed security
 * sector can be served. sub_000E571D reads it with 0xAD READ DVD STRUCTURE,
 * then calls sub_000E5666, which SHA1s 0x4CB bytes and verifies the PKCS#1
 * signature with the XBE's public key. With no signed sector that check can
 * never pass and the title exits via HalReturnToFirmware.
 *
 * The drive path is still implemented in kernel_bridge.c (it serves SS.bin
 * when present). This override only short-circuits the signature result so
 * the title can continue; it reports itself once, loudly. */

void sub_000E5666(void)
{
    static int logged;
    if (!logged) {
        logged = 1;
        fprintf(stderr, "[DVD] media-auth signature check bypassed "
                        "(image has no signed security sector)\n");
        fflush(stderr);
    }
    g_eax = 0;    /* STATUS_SUCCESS: sub_000E571D's caller accepts eax >= 0 */
    /* The real function ends in `ret 4` (text.asm:163341, bytes c2 04 00):
       pop the return address AND the one pushed argument (8 bytes total).
       Popping only 4 shifts the caller's saved ebx/esi/edi and makes the boot
       stub see a bogus failure -> spurious XapiBootToDash + reboot. */
    g_esp += 8;
}
