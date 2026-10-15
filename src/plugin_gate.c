/* =====================================================================
 * SMT IV 60 fps - frame-time logic gate (part of the 3GX plugin)
 *
 * The core runs logic on every other displayed frame.  When a busy scene
 * misses vsync and frames take two vblanks, logic then runs at 15 Hz and the
 * game slows to half speed (the original game kept 30 Hz).  plg_gate runs
 * before the core's hk_frame_begin (0x101B8C) and makes a frame a logic frame
 * when a whole logic tick (two vblanks) of real time has passed since the last
 * one: at 60 fps nothing changes, at 30 fps every frame runs logic.  It steers
 * the core by the parity of the frame counter (logic = even), so the core's
 * tick numbers (counter >> 1) still count logic ticks.  The every-frame time
 * step that hk_task_gate publishes (0.5) is loaded from DT_EVERY instead
 * (plg_main patches that instruction): 1.0 when this frame and the one
 * before both run logic, since a frame is then a whole tick.
 * ===================================================================== */
typedef unsigned int u32;
typedef unsigned char u8;

#pragma clang section text=".plg.gate" bss=".plg.gate.bss"

#define STATE 0x004EFE38u
#define S_COUNTER (*(volatile u32 *)(STATE + 0x00))
#define DT_EVERY  (*(volatile u32 *)(STATE + 0x1C0))   /* 0x4EFFF8: free after State (ends 0x4EFFF4) */
#define F_HALF 0x3F000000u
#define F_ONE  0x3F800000u

#define T_LOGIC   8962315u           /* 2 vblanks: 2 * 16713.74 us at 268111856 Hz */
#define T_DUE     7797214u           /* 0.87 * T_LOGIC (29 ms): a 60 fps frame is 16.7 ms */
static u32 g_last, g_acc, g_prev_logic;

static inline u32 sys_tick(void)
{
    register u32 r0 __asm__("r0");
    register u32 r1 __asm__("r1");
    __asm__ volatile("svc 0x28" : "=r"(r0), "=r"(r1) : : "r2", "r3", "r12", "memory");
    (void)r1;
    return r0;
}

void plg_gate(void)
{
    u32 now = sys_tick();
    u32 c = S_COUNTER;
    int natural = c & 1;                         /* the core makes counter c+1; even = logic */
    int logic = natural;
    if (g_last) {
        u32 el = now - g_last;
        if (el > 4 * T_LOGIC) el = T_LOGIC;      /* load, HOME menu, sleep: one tick */
        g_acc += el;
        if (g_acc >= T_DUE && !natural) { S_COUNTER = c + 1; logic = 1; }
        if (logic) g_acc = g_acc > T_LOGIC ? g_acc - T_LOGIC : 0;
        if (g_acc > T_LOGIC) g_acc = T_LOGIC;    /* never more than one tick behind */
    }
    g_last = now;
    DT_EVERY = logic && g_prev_logic ? F_ONE : F_HALF;
    g_prev_logic = logic;
}

__asm__(
"    .section .plg.gate,\"ax\",%progbits\n"
"    .arm\n"
/* bl (via trampoline) from 0x101B8C, replaces the core's "bl hk_frame_begin" */
"    .global plg_frame_begin\n"
"plg_frame_begin:\n"
"    push  {r0, r1, r2, r3, r12, lr}\n"
"    bl    plg_gate\n"
"    pop   {r0, r1, r2, r3, r12, lr}\n"
"    ldr   pc, =HkFrameBegin\n"             /* the core's hk_frame_begin, lr = main loop */
"    .ltorg\n"
);
