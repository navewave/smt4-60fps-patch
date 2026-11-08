/* =====================================================================
 * SMT IV 60 fps - Luma3DS 3GX plugin: the Azahar extension on a real 3DS
 *
 * Install on hardware:
 *   /luma/titles/<tid>/code.ips       the Real_3DS patch (60 fps core)
 *   /luma/titles/<tid>/exheader.bin   bss +0x5000: pages 0x4F0000-0x4F5000
 *   /luma/plugins/<tid>/smt4_60fps.3gx   this plugin
 *
 * The Azahar build puts the extension (patch/interp_ext.c), the right-stick
 * camera page (src/cam_stick.c) and a few emulator-only hook words into the
 * game with its code.ips.  On hardware Luma cannot apply that part (a
 * code.ips cannot write past the code image), and the extra pages are mapped
 * no-execute.  Luma's plugin loader maps this plugin at 0x07000000, sets the
 * whole game process to RWX and calls plg_start (0x07000100) on the game's
 * main thread before the game's entry point.  plg_main then:
 *
 *   1. writes the difference between the Azahar code.ips and the Real_3DS
 *      code.ips (tools/build_plugin.py makes the table): every word is first
 *      compared with what the Real_3DS patch left there, and nothing is
 *      written unless all match and the extension pages are mapped;
 *   2. sets S->emu (+ emuChecked) so the core calls the extension, and EXT_HW
 *      so the extension flushes the data cache when it patches code;
 *   3. installs the frame-time logic gate (src/plugin_gate.c), hardware only
 *      for now.
 * ===================================================================== */
typedef unsigned int u32;
typedef unsigned char u8;

#pragma clang section text=".plg.entry" bss=".plg.entry.bss" rodata=".plg.entry.rodata"

#include "plg_cfg.h"     /* EXT_PATCH[], EXT_MAP_LAST, GATE_*, PLG_TRAMP (tools/build_plugin.py) */

#define STATE 0x004EFE38u
volatile u32 plg_installed;     /* bit0 extension, bit1 logic gate, bit2 extension pages missing (GDB probes) */

/* svcQueryMemory: 1 if addr is mapped read/write */
static int mapped_rw(u32 addr)
{
    register u32 r0 __asm__("r0");
    register u32 r1 __asm__("r1");
    register u32 r2 __asm__("r2") = addr;
    register u32 r3 __asm__("r3");
    __asm__ volatile("svc 0x02" : "=r"(r0), "=r"(r1), "+r"(r2), "=r"(r3) : : "r4", "r5", "r12", "memory");
    (void)r1; (void)r2;
    return r0 == 0 && (r3 & 3) == 3;
}

/* same check as is_emulator() in patch/interp_thumb.c: svcGetSystemInfo(0x20000, 0) */
static int on_emulator(void)
{
    register u32 r0 __asm__("r0");
    register u32 r1 __asm__("r1") = 0x20000;
    register u32 r2 __asm__("r2") = 0;
    __asm__ volatile("svc 0x2A" : "=r"(r0), "+r"(r1), "+r"(r2) : : "r3", "r12", "memory");
    return r0 == 0 && (r1 == 1 || r1 == 2);
}

/* table: { addr, len, data[(len+3)/4 words], expected[(len+3)/4 words] } ..., 0 */
static int ext_check(void)
{
    for (const u32 *p = EXT_PATCH; p[0]; ) {
        u32 a = p[0], n = p[1], w = (n + 3) / 4;
        const u8 *exp = (const u8 *)(p + 2 + w);
        for (u32 k = 0; k < n; k++) if (((volatile u8 *)a)[k] != exp[k]) return 0;
        p += 2 + 2 * w;
    }
    return 1;
}
static void ext_apply(void)
{
    for (const u32 *p = EXT_PATCH; p[0]; ) {
        u32 a = p[0], n = p[1], w = (n + 3) / 4;
        const u8 *src = (const u8 *)(p + 2);
        for (u32 k = 0; k < n; k++) ((volatile u8 *)a)[k] = src[k];
        p += 2 + 2 * w;
    }
}

extern void plg_frame_begin(void);   /* src/plugin_gate.c */

void plg_main(void)
{
    u32 ok = 0;
    if (!mapped_rw(EXT_MAP_FIRST) || !mapped_rw(EXT_MAP_LAST)) ok |= 4;   /* no exheader.bin: no extension */
    else if (EXT_PATCH[0] && ext_check()) {      /* empty table: built with --no-ext */
        ext_apply();
        *(volatile u8 *)(STATE + 0x83) = 1;      /* emuChecked */
        *(volatile u8 *)(STATE + 0x82) = 1;      /* emu: the core calls the extension */
        *(volatile u8 *)(STATE + 0x1C4) = 1;     /* EXT_HW (patch/interp_ext.c) */
        ok |= 1;
    }
    volatile u32 *t = (volatile u32 *)PLG_TRAMP;
    if (GATE_CALL != 1u &&                       /* 1: built with --no-gate */
        *(volatile u32 *)GATE_CALL == GATE_CALL_OLD && *(volatile u32 *)GATE_DT == GATE_DT_OLD && !t[0] && !t[1]) {
        t[0] = 0xE51FF004u;                      /* ldr pc, [pc, #-4] */
        t[1] = (u32)plg_frame_begin;
        *(volatile u32 *)(STATE + 0x1C0) = 0x3F000000u;   /* every-frame dt 0.5 until the gate runs */
        *(volatile u32 *)GATE_CALL = GATE_CALL_NEW;      /* bl trampoline */
        *(volatile u32 *)GATE_DT = GATE_DT_NEW;          /* ldr r2, [r1, #0x1C0] */
        ok |= 2;
    }
    plg_installed = ok;
    if (!on_emulator()) __asm__ volatile("svc 0x92" : : : "r0", "r1", "r2", "r3", "r12", "memory"); /* clean D-cache */
    __asm__ volatile("svc 0x94" : : : "r0", "r1", "r2", "r3", "r12", "memory");                    /* drop I-cache */
}

/* 0x07000100: called with blx by the loader stub, which restores the game's
 * entry and returns into it afterwards.  Keep the stack 8-byte aligned. */
__asm__(
"    .section .plg.start,\"ax\",%progbits\n"
"    .arm\n"
"    .global plg_start\n"
"plg_start:\n"
"    push  {r4, lr}\n"
"    bl    plg_main\n"
"    pop   {r4, pc}\n"
/* exeLoadFunc payload: copied into the loader, must return exeLoadChecksum (0).
 * Terminated by a NOP; never executed in place. */
"    .section .plg.exeload,\"a\",%progbits\n"
"    .global plg_exeload\n"
"plg_exeload:\n"
"    mov   r0, #0\n"
"    bx    lr\n"
"    nop\n"
);
