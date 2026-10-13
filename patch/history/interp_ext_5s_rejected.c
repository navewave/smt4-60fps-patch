/* =====================================================================
 * Azahar-only extension.  Linked into the last bss page (0x4F0000-0x4F1000),
 * which Azahar's loader allocates before applying code.ips and which the
 * game's crt0 never clears (it stops at 0x4EFE34).  Azahar does not enforce
 * no-execute; a real 3DS does, so the core only calls in here when S->emu
 * was detected at boot.  Hooks into game code that need this region are
 * installed at runtime (ext_install) so the IPS stays harmless on hardware.
 * ===================================================================== */
#include "interp.h"

#ifndef UPDF_MODE
#define UPDF_MODE 3     /* 0 off, 1 hook with old behaviour, 2 "Sk" = smooth, 3 "Ma"/"Vi" = integer */
#endif
/* ---------------------------------------------------------------------
 * Animation frame step (UpdateFrame 0x36880C).  The core scales every
 * step by g_dt, so tasks that run every frame advance anims by half a
 * frame twice.  Right for skeletal motion, but material anims (torch
 * flames: texture flipbook + alpha) on fractional frames misalign.
 * updf_class() decides per anim: 0 = half step (smooth), 1 = integer
 * frames (full step on logic frames, none in between).
 * Names looked at (NW4C): bound AnimGroup (anim+8) -> ResAnimGroup (+8)
 * -> name (+8), and ResAnim (anim+0x54) -> target group name (+0x0C).
 * --------------------------------------------------------------------- */
#pragma clang section text=".ext4.text" rodata=".ext4.rodata"   /* needs the exheader (CAMPAGE) */
static int heap_ptr(u32 p)
{
    if (p & 3) return 0;
    return (p - 0x08000000u < 0x14000000u) || (p - 0x30000000u < 0x10000000u);
}
static u32 bin_str(u32 base, u32 off)               /* self-relative string, 0 if bad */
{
    if (!heap_ptr(base)) return 0;
    int o = *(volatile int *)(base + off);
    if (o == 0 || o < -0x400000 || o > 0x400000) return 0;
    return base + off + o;
}
static u32 name2(u32 str)
{
    if (!str) return 0;
    return *(volatile u8 *)str | (*(volatile u8 *)(str + 1) << 8);
}
#define N2(a, b) ((a) | ((b) << 8))
u32 updf_class(u32 anim)
{
    u32 g = *(volatile u32 *)(anim + 8);
    u32 n1 = heap_ptr(g) ? name2(bin_str(*(volatile u32 *)(g + 8), 8)) : 0;
    u32 n2 = name2(bin_str(*(volatile u32 *)(anim + 0x54), 0x0C));
#if UPDF_MODE == 3
    return n1 == N2('M','a') || n1 == N2('V','i') || n2 == N2('M','a') || n2 == N2('V','i');
#else
    if (n1 == N2('S','k') || n2 == N2('S','k')) return 0;
    return 1;
#endif
}

__asm__(
"    .section .ext.text.updf,\"ax\",%progbits\n"
"    .arm\n"
"    .global ext_updframe\n"
"ext_updframe:\n"                      /* entered by b from 0x36880C */
"    vldr  s0, [r0, #0x40]\n"
"    ldr   r12, =0x004EFE38\n"              /* STATE */
"    vldr  s1, [r12, #8]\n"
"    ldr   r12, [r12, #8]\n"
"    cmp   r12, #0x3F800000\n"
"    beq   UpdateFrame_cont\n"              /* 30 Hz task: full step */
#if UPDF_MODE >= 2
"    push  {r0, r1, r2, r3, lr}\n"
"    vpush {d0}\n"
"    bl    updf_class\n"
"    mov   r12, r0\n"
"    vpop  {d0}\n"
"    pop   {r0, r1, r2, r3, lr}\n"
"    cmp   r12, #0\n"
"    beq   1f\n"
"    ldr   r12, =0x004EFE38\n"              /* integer frames */
"    ldrb  r12, [r12, #4]\n"
"    cmp   r12, #0\n"
"    bne   UpdateFrame_cont\n"              /* logic frame: full step */
"    mov   r12, #0\n"
"    vmov  s0, r12\n"                       /* in-between: no step */
"    b     UpdateFrame_cont\n"
#endif
"1:  vmul.f32 s0, s0, s1\n"                 /* half step */
"    b     UpdateFrame_cont\n"
"    .ltorg\n"
);

#pragma clang section text=".ext.text" rodata=".ext.rodata"
extern u32 GameAlloc_abs(u32 a, u32 b, u32 c, u32 size, u32 e);

#define THR2 (150.0f * 150.0f)
#define UPDF_SITE 0x0036880Cu
#define CAMPAGE (*(volatile u8 *)0x004F0FC4u)   /* 0 unknown, 1 camera/extension pages present, 2 absent */
static inline float f_of(u32 b) { union { u32 u; float f; } x; x.u = b; return x.f; }
static inline u32 b_of(float f) { union { u32 u; float f; } x; x.f = f; return x.u; }

static const WatchDef WDEF[NWATCH] = {
    { 0x00447E88, 0x1C,  3, 0 },  /* player position (navi map tiles are offset by it)            */
    { 0x0044CDF0, 0x38,  2, 0 },  /* navi map scroll centre: FUN_00231F58 -> *(0x44CDF0)+0x38    */
    { 0x0043E278, 0x28,  2, 1 },  /* camp menu ramps (tile scale/offset), stepped inside CAMPDL  */
    { 0x0044CDF0, 0x1A4, 2, 0 },  /* navi map slide offset (menu open/close)                     */
    { 0x0043E128, 0x18,  2, 2 },  /* InfoWin text x offsets (int px), copied from its slide sprite +0xD0 */
    /* the navi draw is excluded from the generic quad interpolation (NOGEN) */
};

/* ---------------------------------------------------------------------
 * COMBG: bottom-screen background (title/start menu, behind the navi map).
 * Object *(0x43DC20+4), stepped by "COMBG CALC" (FUN_001A090C, 30 Hz),
 * drawn by "COMBG DDRAW" (FUN_001ECC7C, prio 0xE5) at int pixel positions:
 *   5 bouncing dots   x +0x18+i*0x14, y +0x1C+i*0x14
 *   5 bouncing lines  y +0x7C+i*0x14
 *   +0x10 fade level
 * --------------------------------------------------------------------- */
#define COMBG_PTR  0x0043DC24u
#define COMBG_N    16
static const u8 COMBG_OFF[COMBG_N] = {
    0x18, 0x1C, 0x2C, 0x30, 0x40, 0x44, 0x54, 0x58, 0x68, 0x6C,
    0x7C, 0x90, 0xA4, 0xB8, 0xCC, 0x10,
};
typedef struct { u32 addr, tick, sw; u32 last[COMBG_N], cur[COMBG_N]; } BgRec;
#define BG ((BgRec *)0x004F0F00u)        /* zero tail of the extension page, < 0x4F0FF0 */
_Static_assert(0x4F0F00 + sizeof(BgRec) <= 0x4F0FF0, "BgRec");

static void bg_swap(u32 t)
{
    BgRec *b = BG;
    u32 o = *(volatile u32 *)COMBG_PTR;
    if (o < 0x00100000u) { b->addr = 0; return; }
    if (b->tick == t && b->addr == o) return;
    int cont = b->addr == o && b->tick == t - 1;
    u32 sw = 0;
    for (int k = 0; k < COMBG_N; k++) {
        volatile u32 *v = (volatile u32 *)(o + COMBG_OFF[k]);
        u32 c = *v, p = b->last[k];
        b->cur[k] = c; b->last[k] = c;
        if (!cont || c == p) continue;
        float e = f_of(c) - f_of(p);
        if (!(e * e <= 48.0f * 48.0f)) continue;
        *v = b_of((f_of(c) + f_of(p)) * 0.5f);
        sw |= 1u << k;
    }
    b->addr = o; b->tick = t; b->sw = sw;
}

static void bg_restore(void)
{
    BgRec *b = BG;
    if (!b->sw) return;
    for (int k = 0; k < COMBG_N; k++)
        if (b->sw & (1u << k)) *(volatile u32 *)(b->addr + COMBG_OFF[k]) = b->cur[k];
    b->sw = 0;
}

/* ---------------------------------------------------------------------
 * EPL particle effects (update FUN_001929BC, draws FUN_001AAC60 3D /
 * FUN_001AA5A0 2D).  Every particle is its own effect node whose data
 * block (node+0x34) the draws read directly:
 *   +0x004..0x00C  int  base translation
 *   +0x120..0x128  float position (integrated by the update)
 *   +0x1F4..0x1FC  float scale (uniform, x, y)
 *   +0x200         float rotation (degrees, wrapped by the draw)
 *   +0x204..0x20C  float offset
 * The allocation is enlarged from 0x3F0 to 0x470 (static patch of the
 * create site) and the record below lives in the extra 0x70 bytes, which
 * the create site zero-fills.
 * --------------------------------------------------------------------- */
#define EPL_UPDATE   0x001929BCu
#define EPL_ORIGWORD 0xE92D40F0u        /* push {r4-r7,lr} */
#define EPL_REC      0x3F0
#define EPL_NF       13
#define EPL_NFLOAT   10
#define EFF_LISTS    0x004C5830u        /* 5 lists x {head, tail, count, -} */
typedef struct {
    u32 tick;                           /* tick of the last recorded update, 0 = invalid */
    u16 sw, pad;                        /* fields currently holding the midpoint */
    u32 pre[EPL_NF];                    /* before the first update this tick */
    u32 post[EPL_NF];                   /* after the last update this tick (true value) */
    u32 psg, ptick;                     /* move direction per field (2 bits) of the last blended tick */
    u32 pad2[2];
} EplRec;                               /* 0x80 bytes (allocation 0x3F0 -> 0x470) */
_Static_assert(sizeof(EplRec) == 0x80, "EplRec size");

static const u16 EPL_OFF[EPL_NF] = {
    0x120, 0x124, 0x128, 0x1F4, 0x1F8, 0x1FC, 0x200, 0x204, 0x208, 0x20C,   /* floats */
    0x004, 0x008, 0x00C,                                                    /* ints   */
};

#define EPL_R(d) ((EplRec *)((d) + EPL_REC))
#define EPL_ANY  (*(volatile u32 *)0x004F0FBCu)   /* some EPL field holds a midpoint */
#define W(d, o)  (*(volatile u32 *)((d) + (o)))

/* called before the original update; returns 1 if the update may be recorded */
u32 epl_pre(u32 d)
{
    EplRec *R = EPL_R(d);
    /* end of life: the update respawns (position snap) or frees the block */
    if ((u32)*(volatile unsigned short *)(d + 0x2CC) == W(d, 0)) { R->tick = 0; return 0; }
    u32 t = tick_now() + 1;             /* +1 so that 0 stays "invalid" */
    if (R->sw) {                        /* updated while swapped: put the truth back first */
        for (int k = 0; k < EPL_NF; k++) if (R->sw & (1u << k)) W(d, EPL_OFF[k]) = R->post[k];
        R->sw = 0;
    }
    if (R->tick != t) {                 /* group 3 is updated twice per tick: keep the first pre */
        for (int k = 0; k < EPL_NF; k++) R->pre[k] = W(d, EPL_OFF[k]);
        R->tick = t;
    }
    return 1;
}

void epl_post(u32 d)
{
    EplRec *R = EPL_R(d);
    for (int k = 0; k < EPL_NF; k++) R->post[k] = W(d, EPL_OFF[k]);
}


static void epl_swap(u32 t)
{
    t += 1;
    for (int g = 0; g < 5; g++) {
        for (u32 n = *(volatile u32 *)(EFF_LISTS + g * 0x10); n >= 0x00100000u; n = W(n, 0x20)) {
            if (W(n, 8) != EPL_UPDATE || *(volatile unsigned char *)(n + 2)) continue;  /* not EPL / dying */
            u32 d = n + 0x34;
            EplRec *R = EPL_R(d);
            if (R->tick != t || R->sw) continue;
            /* direction of this tick's change per field; a field is blended
             * only if it changed the same way last tick (smooth motion).
             * Flickering flame frames (scale/rotation/offset toggling) keep
             * their original choppy look instead of getting in-between states. */
            u32 sw = 0, sg = 0, psg = R->ptick + 1 == t ? R->psg : 0;
            for (int k = 0; k < EPL_NF; k++) {
                u32 c = W(d, EPL_OFF[k]), p = R->pre[k];
                if (c == p || c != R->post[k]) continue;     /* unchanged, or changed since by other code */
                u32 dir = k < EPL_NFLOAT ? (f_of(c) > f_of(p) ? 1u : 2u) : ((int)c > (int)p ? 1u : 2u);
                sg |= dir << (2 * k);
                if (((psg >> (2 * k)) & 3u) != dir) continue;
                u32 m;
                if (k < EPL_NFLOAT) {
                    float e = f_of(c) - f_of(p);
                    if (!(e * e <= 256.0f * 256.0f)) continue;   /* snap / NaN: no blend */
                    m = b_of((f_of(c) + f_of(p)) * 0.5f);
                } else {
                    int e = (int)c - (int)p;
                    if (e > 256 || e < -256) continue;
                    m = (u32)((int)p + e / 2);
                }
                W(d, EPL_OFF[k]) = m;
                sw |= 1u << k;
            }
            R->sw = sw; R->psg = sg; R->ptick = t;
            if (sw) EPL_ANY = 1;
        }
    }
}

/* The EPL draws (2D FUN_001AA5A0 / 3D FUN_001AAC60) transform the 4 quad
 * corners at +0x388 (stride 0x18) IN PLACE when (+0x130 & 1) == 0 and
 * +0x380 != 0; the update re-copies them from the template at +0x26C each
 * tick.  At 60 fps the in-between frame drew them a second time from the
 * already transformed corners (flames whose shape and alpha did not line
 * up).  Put the template corners back at the end of every frame. */
static void epl_reset_corners(void)
{
    for (int g = 0; g < 5; g++)
        for (u32 n = *(volatile u32 *)(EFF_LISTS + g * 0x10); n >= 0x00100000u; n = W(n, 0x20)) {
            if (W(n, 8) != EPL_UPDATE) continue;
            u32 d = n + 0x34;
            if ((*(volatile u8 *)(d + 0x130) & 1) || !*(volatile u8 *)(d + 0x380)) continue;
            for (int i = 0; i < 4; i++)
                for (int k = 0; k < 3; k++)
                    W(d, 0x388 + i * 0x18 + k * 4) = W(d, 0x26C + i * 0x18 + k * 4);
        }
}

static void epl_restore(void)
{
    if (!EPL_ANY) return;
    EPL_ANY = 0;
    for (int g = 0; g < 5; g++) {
        for (u32 n = *(volatile u32 *)(EFF_LISTS + g * 0x10); n >= 0x00100000u; n = W(n, 0x20)) {
            if (W(n, 8) != EPL_UPDATE) continue;
            u32 d = n + 0x34;
            EplRec *R = EPL_R(d);
            if (!R->sw) continue;
            for (int k = 0; k < EPL_NF; k++) if (R->sw & (1u << k)) W(d, EPL_OFF[k]) = R->post[k];
            R->sw = 0;
        }
    }
}

/* ---------------------------------------------------------------------
 * Runtime hook installation (Azahar only).  svc 0x93 is
 * InvalidateInstructionCacheRange, which Azahar forwards to the JIT.
 * --------------------------------------------------------------------- */
extern char ext_epl_hook[];

static inline void put_branch(u32 site, u32 to)
{
    *(volatile u32 *)site = 0xEA000000u | (((to - (site + 8)) >> 2) & 0xFFFFFFu);
}
static void icache_inval(u32 addr, u32 size)
{
    register u32 r0 __asm__("r0") = addr;
    register u32 r1 __asm__("r1") = size;
    __asm__ volatile("svc 0x93" : "+r"(r0), "+r"(r1) : : "r2", "r3", "r12", "memory");
}

/* ---------------------------------------------------------------------
 * Big sprite moves.  The core blends animator moves up to 256 px per tick
 * (fast slides).  A move over 64 px is kept only if the sprite was already
 * moving that way the tick before; otherwise it is an animation start
 * (sprite jumps to its first keyframe) and the core's midpoint is undone,
 * which removes the one-frame flash halfway between the old spot and the
 * start of the slide.
 * --------------------------------------------------------------------- */
typedef struct { u32 spr, tick; int dx, dy; } SHist;
#define SH_MAGIC 0x53484953u
#define SH_HEAP  (*(volatile u32 *)0x004F0FC0u)
#define BIG      (64 << 12)
static int sh_ok(int d, int p)
{
    if (d <= BIG && d >= -BIG) return 1;
    if (d > 0) return p > 0 && d <= p * 3 + (2 << 12);
    return p < 0 && d >= p * 3 - (2 << 12);
}
static void spr_bigmove_fix(SprTab *T, u32 t)
{
    u32 hp = SH_HEAP;
    if (hp == 1) return;
    if (!hp || *(u32 *)hp != SH_MAGIC) {
        hp = GameAlloc_abs(0x100, 0x100, 0, 16 + SPR_N * sizeof(SHist), 1);
        if (hp < 0x00100000u) { SH_HEAP = 1; return; }
        *(u32 *)hp = SH_MAGIC; SH_HEAP = hp;
    }
    SHist *H = (SHist *)(hp + 16);
    for (int i = 0; i < SPR_N; i++) {
        SRec *r = &T->e[i];
        if (!r->spr || r->tick != t) continue;
        int dx = r->qx - r->px, dy = r->qy - r->py;
        SHist *h = &H[i];
        int cont = h->spr == r->spr && h->tick + 1 == t;
        int pdx = cont ? h->dx : 0, pdy = cont ? h->dy : 0;
        h->spr = r->spr; h->tick = t; h->dx = dx; h->dy = dy;
        if ((r->swapped & 1) && (!sh_ok(dx, pdx) || !sh_ok(dy, pdy))) {
            SPX(r->spr) = r->cx; SPY(r->spr) = r->cy;     /* undo the position midpoint only */
        }
    }
}

/* EPL update wrapper: replays the displaced "push {r4-r7,lr}" */
__asm__(
"    .section .ext.text.epl,\"ax\",%progbits\n"
"    .arm\n"
"    .global ext_epl_hook\n"
"ext_epl_hook:\n"
"    push  {r0, r1, r2, lr}\n"
"    bl    epl_pre\n"
"    str   r0, [sp, #4]\n"
"    ldr   r0, [sp]\n"
"    bl    1f\n"
"    ldr   r1, [sp, #4]\n"
"    cmp   r1, #0\n"
"    ldrne r0, [sp]\n"
"    blne  epl_post\n"
"    pop   {r0, r1, r2, pc}\n"
"1:  push  {r4, r5, r6, r7, lr}\n"
"    b     EplUpdate_cont\n"
"    .ltorg\n"
);

/* ---------------------------------------------------------------------
 * COMBG: common bottom-screen background (navi map, start menu): 5 dots
 * (x +0x18, y +0x1C) and 5 lines (y +0x7C), stride 0x14, object
 * *(0x43DC24), moved by "COMBG CALC" at 30 Hz and drawn at whole pixels
 * by FUN_001ECC7C.  bg_swap() puts the midpoint into the object on logic
 * frames; the two cell draws are redirected here so the fraction of a
 * pixel is kept (SUBPX, added to the quad position by ext_quad_*), which
 * Azahar's resolution scaling can show.
 * --------------------------------------------------------------------- */
#define COMBG_LINE_CALL 0x001ECE64u
#define COMBG_DOT_CALL  0x001ECF54u
#define COMBG_BL_TARGET 0x002BF460u      /* both sites are "bl FUN_002BF460" */
extern void CellPut(u32 mode, u32 sheet, u32 cell, int x, int y, u32 prio);   /* FUN_002BF460 */
extern char ext_combg_line[], ext_combg_dot[], ext_quad[], ext_navi[];
/* Sub-pixel offset for the COMBG cells (added by quad_interp, which then
 * leaves those quads alone: bg_swap already interpolated them). */
#define SUBPX   ((volatile float *)0x004F0FA0u)   /* after BgRec (0x4F0F00-0x4F0F8C) */
#define SUBPX_ON (*(volatile u32 *)0x004F0FB0u)
#define NOGEN    (*(volatile u32 *)0x004F0FB8u)   /* 1 while the navi map draws (watch-interpolated) */
#define NAVI_CALL 0x001FC4ACu                      /* bl FUN_0039F238 in "NAVI DOWN" FUN_001FC480 */
#define NAVI_BL_TARGET 0x0039F238u

void combg_draw(u32 dot, u32 e, u32 prio, u32 i)   /* i = loop index (r4 in both loops) */
{
    /* on logic frames bg_swap() has already put the midpoint into the object */
    float x = dot ? *(volatile float *)(e + 0x18) : 0.0f;
    float y = *(volatile float *)(e + (dot ? 0x1C : 0x7C));
    int ix = (int)x, iy = (int)y;
    SUBPX[0] = x - (float)ix; SUBPX[1] = y - (float)iy;   /* added to the quad by quad_interp */
    SUBPX_ON = 1;
    CellPut(2, 0x11, dot ? 1 : 3, ix, iy, prio);
    SUBPX_ON = 0;
}

__asm__(
"    .section .ext.text.combg,\"ax\",%progbits\n"
"    .arm\n"
"    .global ext_combg_line, ext_combg_dot\n"
"ext_combg_line:\n"                    /* r6 = line element, [sp+4] = prio */
"    ldr   r2, [sp, #4]\n"
"    mov   r3, r4\n"
"    mov   r1, r6\n"
"    mov   r0, #0\n"
"    b     combg_draw\n"
"ext_combg_dot:\n"                     /* r5 = object, r4 = index */
"    ldr   r2, [sp, #4]\n"
"    add   r1, r4, r4, lsl #2\n"
"    add   r1, r5, r1, lsl #2\n"
"    mov   r3, r4\n"
"    mov   r0, #1\n"
"    b     combg_draw\n"
"    .global ext_navi\n"
"ext_navi:\n"                          /* replaces bl FUN_0039F238 (r0 = navi+0x40) */
"    push  {r4, lr}\n"
"    ldr   r12, =0x004F0FB8\n"
"    mov   r4, #1\n"
"    str   r4, [r12]\n"
"    ldr   r12, =0x0039F238\n"
"    blx   r12\n"
"    ldr   r12, =0x004F0FB8\n"
"    mov   r4, #0\n"
"    str   r4, [r12]\n"
"    pop   {r4, pc}\n"
"    .ltorg\n"
);

/* ---------------------------------------------------------------------
 * Generic 2D interpolation.  Every 2D quad (cells, layouts, windows, the
 * navi map ...) is drawn by FUN_002B1590(col0..col3 in r0-r3, x s0, y s1,
 * w s2, h s3, u s4, v s5, du s6, dv s7).  A quad is identified by its
 * texture (ctx+0x11E4), uv and size plus how many identical quads came
 * before it in the frame.  In-between frames (true state) record the
 * position; on logic frames a quad that moved since then AND was already
 * moving the same way the tick before (a slide, not a jump) is drawn at
 * the midpoint.
 * --------------------------------------------------------------------- */
#define QUAD_FN        0x002B1590u
#define SPRDRAW_A      0x002B9708u        /* cmp r0, #0 */
#define SPRDRAW_B      0x003883C0u        /* cmp r3, #0 */
extern char ext_sprdraw_a[], ext_sprdraw_b[], ext_bust[];
#define BUST_FN        0x001DBB54u        /* push {r4, r5, r6, lr} */
#define QUAD_ORIGWORD  0xEE305A01u        /* vadd.f32 s10, s0, s2 */
#define Q_CNT   256
#define Q_REC   1024
#define Q_MAGIC 0x51554144u
#define Q_MAXD  192.0f
typedef struct { u32 base, stamp, n; } QCnt;
typedef struct { u32 key, tick; float x, y, dx, dy, w, h, dw, dh; } QRec;
typedef struct { u32 magic, pad[3]; QCnt c[Q_CNT]; QRec r[Q_REC]; } QTab;
#define Q_HEAP (*(volatile u32 *)0x004F0FB4u)
extern u32 GameAlloc_abs(u32 a, u32 b, u32 c, u32 size, u32 e);

#pragma clang section text=".ext4.text" rodata=".ext4.rodata"   /* extra page 0x4F2000: needs the exheader */
static __attribute__((noinline)) QTab *q_tab(void)
{
    u32 p = Q_HEAP;
    if (p == 1) return 0;
    if (p && ((QTab *)p)->magic == Q_MAGIC) return (QTab *)p;
    p = GameAlloc_abs(0x100, 0x100, 0, sizeof(QTab), 1);
    if (p < 0x00100000u) { Q_HEAP = 1; return 0; }
    ((QTab *)p)->magic = Q_MAGIC;
    Q_HEAP = p;
    return (QTab *)p;
}

/* this tick's move d continues last tick's move p: same direction and at
 * most ~2.5x as far (slides ease in/out; loops that wrap and quads that got
 * matched to a different element do not) */
static __attribute__((noinline)) int q_ok(float d, float p)
{
    if (d == 0.0f) return 1;
    if (d > 0.0f) return p > 0.0f && d <= p * 2.5f + 2.0f;
    return p < 0.0f && d >= p * 2.5f - 2.0f;
}

void quad_interp(float *f)               /* f = saved s0..s7; only called when the camera page exists */
{
    if (SUBPX_ON) { f[0] += SUBPX[0]; f[1] += SUBPX[1]; return; }
    if (NOGEN) return;
    QTab *T = q_tab();
    if (!T) return;
    u32 *b = (u32 *)f;
    u32 tex = *(volatile u32 *)(*(volatile u32 *)0x0044EA00u + 0x11E4);
    /* identity: texture + size + uv (5i rules: sizes are not interpolated) */
    u32 base = tex * 0x9E3779B1u ^ b[2] * 0x85EBCA77u ^ b[3] * 0xC2B2AE3Du ^ b[4] * 0x27D4EB2Fu ^ b[5] * 0x165667B1u;
    base |= 1;
    /* occurrence of this exact quad within the frame */
    u32 stamp = S->counter, occ = 0;
    u32 h = base >> 24;
    for (int k = 0; k < 8; k++) {
        QCnt *c = &T->c[(h + k) & (Q_CNT - 1)];
        if (c->stamp != stamp) { c->stamp = stamp; c->base = base; c->n = 1; occ = 0; goto found; }
        if (c->base == base) { occ = c->n++; goto found; }
    }
    return;
found:;
    u32 key = base + occ * 0x61C88647u;
    if (!key) key = 1;
    u32 hr = (key >> 22) & (Q_REC - 1);
    u32 t = tick_now() + 1;
    QRec *r = 0, *fr = 0;
    for (int k = 0; k < 8; k++) {
        QRec *q = &T->r[(hr + k) & (Q_REC - 1)];
        if (q->key == key) { r = q; break; }
        if (!fr && (q->key == 0 || q->tick + 2 < t)) fr = q;
    }
    float x = f[0], y = f[1];
    if (!S->logic) {                                   /* true state: record */
        if (!r) { if (!fr) return; r = fr; r->key = key; r->tick = 0; }
        if (r->tick == t) return;                      /* drawn twice: keep the first */
        if (r->tick + 1 == t) { r->dx = x - r->x; r->dy = y - r->y; r->dw = f[2] - r->w; r->dh = f[3] - r->h; }
        else { r->dx = r->dy = r->dw = r->dh = 0.0f; }
        r->x = x; r->y = y; r->w = f[2]; r->h = f[3]; r->tick = t;
        return;
    }
    if (!r || r->tick + 1 != t) return;                /* logic frame */
    float dx = x - r->x, dy = y - r->y;
    if (dx == 0.0f && dy == 0.0f) return;
    if (dx > Q_MAXD || dx < -Q_MAXD || dy > Q_MAXD || dy < -Q_MAXD) return;
    if (!q_ok(dx, r->dx) || !q_ok(dy, r->dy)) return;     /* only slides that continue */
    f[0] = r->x + dx * 0.5f;
    f[1] = r->y + dy * 0.5f;
}

__asm__(
"    .section .ext.text.quad,\"ax\",%progbits\n"
"    .arm\n"
"    .global ext_quad\n"
"ext_quad:\n"                          /* entered by b from 0x2B1590 */
"    push  {r0, r1, r2, r3, r12, lr}\n"
"    vpush {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    ldr   r12, =0x004F0FC4\n"              /* CAMPAGE: quad_interp lives in the camera page */
"    ldrb  r12, [r12]\n"
"    mov   r0, sp\n"
"    cmp   r12, #1\n"
"    bleq  quad_interp\n"
"    vpop  {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    pop   {r0, r1, r2, r3, r12, lr}\n"
"    vadd.f32 s10, s0, s2\n"
"    b     QuadFn_cont\n"
"    .ltorg\n"
);

#pragma clang section text=".ext.text" rodata=".ext.rodata"
extern u32 GameAlloc_abs(u32 a, u32 b, u32 c, u32 size, u32 e);
/* replace "bl target" at site by "bl to" (version independent: checks the
 * decoded target instead of the raw word) */
static void hook_call(u32 site, u32 target, u32 to)
{
    u32 w = *(volatile u32 *)site;
    if ((w >> 24) != 0xEBu || site + 8 + ((int)(w << 8) >> 6) != target) return;
    *(volatile u32 *)site = 0xEB000000u | (((to - (site + 8)) >> 2) & 0xFFFFFFu);
    icache_inval(site, 4);
}

static void ext_install(void)
{
    hook_call(COMBG_LINE_CALL, COMBG_BL_TARGET, (u32)ext_combg_line);
    hook_call(COMBG_DOT_CALL,  COMBG_BL_TARGET, (u32)ext_combg_dot);
    hook_call(NAVI_CALL, NAVI_BL_TARGET, (u32)ext_navi);
    if (CAMPAGE == 1 && *(volatile u32 *)BUST_FN == 0xE92D4070u) { put_branch(BUST_FN, (u32)ext_bust); icache_inval(BUST_FN, 4); }
    {   /* anim step: replace the core's "b hk_updframe" */
        extern char hk_updframe[], ext_updframe[];
        u32 w = *(volatile u32 *)UPDF_SITE;
        if (UPDF_MODE && CAMPAGE == 1 && (w >> 24) == 0xEAu && UPDF_SITE + 8 + ((int)(w << 8) >> 6) == (u32)hk_updframe) {
            put_branch(UPDF_SITE, (u32)ext_updframe); icache_inval(UPDF_SITE, 4);
        }
    }
    if (*(volatile u32 *)QUAD_FN == QUAD_ORIGWORD) { put_branch(QUAD_FN, (u32)ext_quad); icache_inval(QUAD_FN, 4); }
    volatile u32 *site = (volatile u32 *)EPL_UPDATE;
    if (*site != EPL_ORIGWORD) return;                       /* installed (or not our code) */
    /* the create site must have been enlarged (static IPS patch), else stay off */
    if (*(volatile u32 *)0x002D822Cu != 0xE3A02E47u) return;
    /* and no particle may still exist from before that (cheat builds patch it
     * late): wait until there are none, then every block has the record */
    for (int g = 0; g < 5; g++)
        for (u32 n = *(volatile u32 *)(EFF_LISTS + g * 0x10); n >= 0x00100000u; n = W(n, 0x20))
            if (W(n, 8) == EPL_UPDATE) return;
    put_branch(EPL_UPDATE, (u32)ext_epl_hook);
    icache_inval(EPL_UPDATE, 4);
}

/* ---------------------------------------------------------------------
 * Field 2D tiles (entrances, shops, event/search markers): the "MIF3DL"
 * draw (0x6F, FUN_001E83FC) renders billboards straight from vertex
 * arrays that "MAPIF C" fills at 30 Hz: tile e = *(0x447A78) + i*0x194
 * + 0x1D0 (128 tiles, active if *(u8*)e), five quads of 4 xyz vertices
 * at e+0x90..e+0x180 (60 floats).  Midpoint swap like the sprites.
 * --------------------------------------------------------------------- */
#define MIF_N      128
#define MIF_SLOTS  32
#define MIF_NF     60
#define MIF_MAGIC  0x4D494631u
typedef struct { u32 key, tick, sw; float cur[MIF_NF]; } MifRec;
typedef struct { u32 magic, pad[3]; MifRec r[MIF_SLOTS]; } MifTab;
#define MIF_HEAP (*(volatile u32 *)0x004F0FA8u)
extern u32 GameAlloc_abs(u32 a, u32 b, u32 c, u32 size, u32 e);

#pragma clang section text=".ext4.text" rodata=".ext4.rodata"   /* needs the exheader (CAMPAGE) */
static MifTab *mif_tab(void)
{
    u32 p = MIF_HEAP;
    if (p == 1) return 0;                                   /* allocation failed once: off */
    if (p && ((MifTab *)p)->magic == MIF_MAGIC) return (MifTab *)p;
    p = GameAlloc_abs(0x100, 0x100, 0, sizeof(MifTab), 1);
    if (p < 0x00100000u) { MIF_HEAP = 1; return 0; }
    ((MifTab *)p)->magic = MIF_MAGIC;
    MIF_HEAP = p;
    return (MifTab *)p;
}

void mif_swap(u32 t)
{
    u32 mif = *(volatile u32 *)0x00447A78u;
    if (mif < 0x00100000u) return;
    MifTab *T = mif_tab();
    if (!T) return;
    t += 1;
    for (u32 i = 0; i < MIF_N; i++) {
        u32 e = mif + i * 0x194 + 0x1D0;
        if (!*(volatile u8 *)e) continue;
        MifRec *r = 0, *fr = 0;
        for (int k = 0; k < 4; k++) {
            MifRec *q = &T->r[(i + k * 7) & (MIF_SLOTS - 1)];
            if (q->key == e) { r = q; break; }
            if (!fr && (q->key == 0 || q->tick + 1 < t)) fr = q;
        }
        volatile float *v = (volatile float *)(e + 0x90);
        if (!r) {
            if (!fr) continue;
            r = fr; r->key = e; r->tick = 0;
        }
        if (r->tick == t || r->sw) continue;
        int ok = r->tick + 1 == t, moved = 0;
        if (ok) for (int k = 0; k < MIF_NF; k++) {
            float d = v[k] - r->cur[k];
            if (!(d * d <= 300.0f * 300.0f)) { ok = 0; break; }
            if (d != 0.0f) moved = 1;
        }
        for (int k = 0; k < MIF_NF; k++) {
            float c = v[k];
            if (ok && moved) v[k] = (c + r->cur[k]) * 0.5f;
            r->cur[k] = c;
        }
        r->tick = t;
        r->sw = ok && moved;
    }
}

void mif_restore(void)
{
    u32 p = MIF_HEAP;
    if (p < 0x00100000u || ((MifTab *)p)->magic != MIF_MAGIC) return;
    MifTab *T = (MifTab *)p;
    for (int k = 0; k < MIF_SLOTS; k++) {
        MifRec *r = &T->r[k];
        if (!r->sw) continue;
        volatile float *v = (volatile float *)(r->key + 0x90);
        for (int j = 0; j < MIF_NF; j++) v[j] = r->cur[j];
        r->sw = 0;
    }
}

/* ---------------------------------------------------------------------
 * 3D node moves (ext3: lives in the camera page, so only with the camera
 * exheader).  Runs at BeforeDraw1L (0x65), before AsdModelCalcProc (0x67)
 * applies the core's node midpoints.  A move is blended only if it
 * continues last tick's move (same direction, at most 3x as far); a jump
 * from rest - e.g. a menu model placed at its slide start - is vetoed by
 * un-stamping its record, so it is not drawn halfway for a frame.
 * --------------------------------------------------------------------- */
/* ---------------------------------------------------------------------
 * Generic sprite-object interpolation (ext4).  Every AsdSDK sprite object
 * (0x8C bytes) is drawn by FUN_002B9708(spr) or FUN_003883C0(.., .., .., spr)
 * (talk bust-ups / Burroughs).  The drawn position is the 20.12 position
 * (+0x0C/+0x10) plus the s16 offset (+0x18/+0x1A), the scale is +0x28/+0x2C.
 * Code that moves/scales sprites directly (not through the hooked
 * animators) left them at 30 Hz.  Here each sprite (and its n-th draw in a
 * frame) is recorded on in-between frames; on logic frames a change that
 * continues last tick's change (or is tiny) is drawn at the midpoint, and
 * the true values are put back right after the draw.  Sprites the core
 * already blended this tick are left alone.
 * --------------------------------------------------------------------- */
#if 0   /* generic sprite interpolation: disabled in 5n (menus got jumpy) */
#define SD_N     512
#define SD_MAGIC 0x53505244u
#define SD_HEAP  (*(volatile u32 *)0x004F0FD0u)
#define SDP      ((volatile u32 *)0x004F0FD4u)     /* pending restore: spr, x, y, sx, sy (5 words) */
typedef struct { u32 key, tick; int x, y, dx, dy; float sx, sy, dsx, dsy; } SdRec;
typedef struct { u32 key, stamp, n; } SdCnt;
typedef struct { u32 magic, pad[3]; SdCnt c[128]; SdRec r[SD_N]; } SdTab;
static __attribute__((noinline)) SdTab *sd_tab(void)
{
    u32 p = SD_HEAP;
    if (p == 1) return 0;
    if (p && *(u32 *)p == SD_MAGIC) return (SdTab *)p;
    p = GameAlloc_abs(0x100, 0x100, 0, sizeof(SdTab), 1);
    if (p < 0x00100000u) { SD_HEAP = 1; return 0; }
    *(u32 *)p = SD_MAGIC; SD_HEAP = p;
    return (SdTab *)p;
}
static int sd_core_swapped(u32 spr, u32 t)
{
    SprTab *T = (SprTab *)S->heap;
    if (!T || T->magic != SPR_MAGIC) return 0;
    u32 h = ((spr >> 2) * 2654435761u) >> 24;
    for (int k = 0; k < 32; k++) {
        SRec *r = &T->e[(h + k) & (SPR_N - 1)];
        if (r->spr == spr && r->tick == t) return (r->swapped & 3) != 0;
    }
    return 0;
}
static int sd_cont_i(int d, int p, int small)
{
    if (d == 0) return 1;
    if (d <= small && d >= -small) return 1;
    if (d > 0) return p > 0 && d <= p * 3 + small;
    return p < 0 && d >= p * 3 - small;
}
static int sd_cont_f(float d, float p, float small)
{
    if (d == 0.0f) return 1;
    if (d <= small && d >= -small) return 1;
    if (d > 0.0f) return p > 0.0f && d <= p * 3.0f + small;
    return p < 0.0f && d >= p * 3.0f - small;
}
void sd_pre(u32 spr)
{
    SDP[0] = 0;
    if (spr < 0x00100000u) return;
    SdTab *T = sd_tab();
    if (!T) return;
    u32 stamp = S->counter, occ = 0, h = (spr >> 3) & 127;
    for (int k = 0; k < 8; k++) {
        SdCnt *c = &T->c[(h + k) & 127];
        if (c->stamp != stamp) { c->stamp = stamp; c->key = spr; c->n = 1; occ = 0; goto found; }
        if (c->key == spr) { occ = c->n++; goto found; }
    }
    return;
found:;
    u32 key = spr + occ * 0x9E3779B1u, t = tick_now() + 1;
    u32 hr = ((key >> 3) * 2654435761u) >> 23;
    SdRec *r = 0, *fr = 0;
    for (int k = 0; k < 8; k++) {
        SdRec *q = &T->r[(hr + k) & (SD_N - 1)];
        if (q->key == key) { r = q; break; }
        if (!fr && (q->key == 0 || q->tick + 2 < t)) fr = q;
    }
    int x = ((int)*(volatile short *)(spr + 0x18) << 12) + *(volatile int *)(spr + 0x0C);
    int y = ((int)*(volatile short *)(spr + 0x1A) << 12) + *(volatile int *)(spr + 0x10);
    float sx = *(volatile float *)(spr + 0x28), sy = *(volatile float *)(spr + 0x2C);
    if (!S->logic) {
        if (!r) { if (!fr) return; r = fr; r->key = key; r->tick = 0; }
        if (r->tick == t) return;
        if (r->tick + 1 == t) { r->dx = x - r->x; r->dy = y - r->y; r->dsx = sx - r->sx; r->dsy = sy - r->sy; }
        else { r->dx = r->dy = 0; r->dsx = r->dsy = 0.0f; }
        r->x = x; r->y = y; r->sx = sx; r->sy = sy; r->tick = t;
        return;
    }
    if (!r || r->tick + 1 != t) return;
    if (sd_core_swapped(spr, tick_now())) return;
    int dx = x - r->x, dy = y - r->y;
    float dsx = sx - r->sx, dsy = sy - r->sy;
    int mp = (dx || dy) && dx <= (256 << 12) && dx >= -(256 << 12) && dy <= (256 << 12) && dy >= -(256 << 12)
             && sd_cont_i(dx, r->dx, 3 << 12) && sd_cont_i(dy, r->dy, 3 << 12);
    float sm = 0.06f * (r->sx > 0.0f ? r->sx : -r->sx) + 0.001f;
    int ms = (dsx != 0.0f || dsy != 0.0f) && sd_cont_f(dsx, r->dsx, sm) && sd_cont_f(dsy, r->dsy, sm);
    if (!mp && !ms) return;
    SDP[0] = spr;
    SDP[1] = *(volatile u32 *)(spr + 0x0C); SDP[2] = *(volatile u32 *)(spr + 0x10);
    SDP[3] = *(volatile u32 *)(spr + 0x28); SDP[4] = *(volatile u32 *)(spr + 0x2C);
    if (mp) { *(volatile int *)(spr + 0x0C) -= dx / 2; *(volatile int *)(spr + 0x10) -= dy / 2; }
    if (ms) { *(volatile float *)(spr + 0x28) = sx - dsx * 0.5f; *(volatile float *)(spr + 0x2C) = sy - dsy * 0.5f; }
}
void sd_post(void)
{
    u32 spr = SDP[0];
    if (!spr) return;
    *(volatile u32 *)(spr + 0x0C) = SDP[1]; *(volatile u32 *)(spr + 0x10) = SDP[2];
    *(volatile u32 *)(spr + 0x28) = SDP[3]; *(volatile u32 *)(spr + 0x2C) = SDP[4];
    SDP[0] = 0;
}
__asm__(
"    .section .ext4.text.sd,\"ax\",%progbits\n"
"    .arm\n"
"    .global ext_sprdraw_a, ext_sprdraw_b\n"
"ext_sprdraw_a:\n"                       /* entered by b from FUN_002B9708 (r0 = sprite, s0 = x offset) */
"    push  {r0, r1, r2, r3, r12, lr}\n"
"    vpush {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    bl    sd_pre\n"
"    vpop  {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    pop   {r0, r1, r2, r3, r12, lr}\n"
"    push  {r0, lr}\n"
"    bl    1f\n"
"    pop   {r0, lr}\n"
"    b     sd_post\n"
"1:  cmp   r0, #0\n"
"    b     SprDrawA_cont\n"
"ext_sprdraw_b:\n"                       /* entered by b from FUN_003883C0 (r3 = sprite) */
"    push  {r0, r1, r2, r3, r12, lr}\n"
"    vpush {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    mov   r0, r3\n"
"    bl    sd_pre\n"
"    vpop  {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    pop   {r0, r1, r2, r3, r12, lr}\n"
"    push  {r3, lr}\n"
"    bl    2f\n"
"    pop   {r3, lr}\n"
"    b     sd_post\n"
"2:  cmp   r3, #0\n"
"    b     SprDrawB_cont\n"
);
#endif
/* ---------------------------------------------------------------------
 * Bust-up / hologram quads (Burroughs on the top screen): FUN_001DBB54(task)
 * draws a billboard quad (FUN_002BA520) from the task work w =
 * FUN_002C0AC8(task): centre x = +0x34 + +0x58, y = +0x38, z = +0x3C,
 * half size +0x1C/+0x20, alpha +0x28.  The scale-in grows the half size at
 * 30 Hz.  Each field that keeps changing the same way is drawn at the
 * midpoint on logic frames (recorded on in-between frames), restored after.
 * --------------------------------------------------------------------- */
#define BU_N   16
#define BU_NF  7
static const u8 BU_OFF[BU_NF] = { 0x1C, 0x20, 0x28, 0x34, 0x38, 0x3C, 0x58 };
typedef struct { u32 w, tick; float v[BU_NF], d[BU_NF]; } BuRec;   /* 64 bytes */
#define BU_HEAP (*(volatile u32 *)0x004F0FF0u)        /* heap: magic + 16 x 64 bytes + 7 saved floats */
#define BU_MAGIC 0x42555354u
#define BUP    ((volatile u32 *)0x004F0FE8u)          /* pending restore: work, mask */
extern u32 TaskWork(u32 handle);                      /* FUN_002C0AC8 */
void bu_pre(u32 handle)
{
    BUP[0] = 0;
    u32 hp = BU_HEAP;
    if (hp == 1) return;
    if (!hp || *(u32 *)hp != BU_MAGIC) {
        hp = GameAlloc_abs(0x100, 0x100, 0, 16 + BU_N * sizeof(BuRec) + 32, 1);
        if (hp < 0x00100000u) { BU_HEAP = 1; return; }
        *(u32 *)hp = BU_MAGIC; BU_HEAP = hp;
    }
    BuRec *BU_TAB = (BuRec *)(hp + 16);
    u32 w = TaskWork(handle);
    if (w < 0x00100000u) return;
    u32 t = tick_now() + 1;
    BuRec *r = 0, *fr = 0;
    for (int k = 0; k < BU_N; k++) {
        if (BU_TAB[k].w == w) { r = &BU_TAB[k]; break; }
        if (!fr && BU_TAB[k].tick + 2 < t) fr = &BU_TAB[k];
    }
    volatile float *f = (volatile float *)w;
    if (!S->logic) {
        if (!r) { if (!fr) return; r = fr; r->w = w; r->tick = 0; }
        if (r->tick == t) return;
        for (int k = 0; k < BU_NF; k++) {
            float c = f[BU_OFF[k] / 4];
            r->d[k] = r->tick + 1 == t ? c - r->v[k] : 0.0f;
            r->v[k] = c;
        }
        r->tick = t;
        return;
    }
    if (!r || r->tick + 1 != t) return;
    u32 mask = 0;
    volatile float *sv = (volatile float *)(hp + 16 + BU_N * sizeof(BuRec));   /* 7 saved floats */
    for (int k = 0; k < BU_NF; k++) {
        float c = f[BU_OFF[k] / 4], d = c - r->v[k], p = r->d[k];
        if (d == 0.0f) continue;
        int ok = d > 0.0f ? (p > 0.0f && d <= p * 3.0f + 1.0f) : (p < 0.0f && d >= p * 3.0f - 1.0f);
        if (!ok) continue;
        sv[k] = c;
        f[BU_OFF[k] / 4] = c - d * 0.5f;
        mask |= 1u << k;
    }
    if (mask) { BUP[0] = w; BUP[1] = mask; }
}
void bu_post(void)
{
    u32 w = BUP[0];
    if (!w) return;
    volatile float *f = (volatile float *)w, *sv = (volatile float *)(BU_HEAP + 16 + BU_N * sizeof(BuRec));
    for (int k = 0; k < BU_NF; k++) if (BUP[1] & (1u << k)) f[BU_OFF[k] / 4] = sv[k];
    BUP[0] = 0;
}
__asm__(
"    .section .ext4.text.bu,\"ax\",%progbits\n"
"    .arm\n"
"    .global ext_bust\n"
"ext_bust:\n"                             /* entered by b from FUN_001DBB54 (r0 = task, s0/s1 = offsets) */
"    push  {r0, r1, r2, r3, r12, lr}\n"
"    vpush {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    bl    bu_pre\n"
"    vpop  {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    pop   {r0, r1, r2, r3, r12, lr}\n"
"    push  {r0, lr}\n"
"    bl    1f\n"
"    pop   {r0, lr}\n"
"    b     bu_post\n"
"1:  push  {r4, r5, r6, lr}\n"
"    b     BustDraw_cont\n"
);
#pragma clang section text=".ext3.text" rodata=".ext3.rodata"
#define ROT_ANY (*(volatile u32 *)0x004F0FCCu)
typedef struct { u32 node, tick; float d[3]; } NHist;
#define NH_N 24
typedef struct { u32 node, tick, sw; float R[9], T[9]; } RHist;
#define RH_N 24
#define X3_MAGIC 0x58334850u
#define X3_HEAP (*(volatile u32 *)0x004F0FC8u)
static __attribute__((noinline)) u32 x3_heap(void)
{
    u32 hp = X3_HEAP;
    if (hp == 1) return 0;
    if (hp && *(u32 *)hp == X3_MAGIC) return hp;
    hp = GameAlloc_abs(0x100, 0x100, 0, 16 + NH_N * sizeof(NHist) + RH_N * sizeof(RHist), 1);
    if (hp < 0x00100000u) { X3_HEAP = 1; return 0; }
    *(u32 *)hp = X3_MAGIC; X3_HEAP = hp;
    return hp;
}
void node_veto(u32 t)
{
    u32 hp = x3_heap();
    if (!hp) return;
    NHist *NH = (NHist *)(hp + 16);
    for (int i = 0; i < NREC; i++) {
        Rec *r = &S->rec[i];
        if (!r->node || r->tick != t) continue;
        u32 n = r->node;
        float d[3] = { *(volatile float *)(n + 0x58) - r->p[0],
                       *(volatile float *)(n + 0x68) - r->p[1],
                       *(volatile float *)(n + 0x78) - r->p[2] };
        NHist *h = 0, *fr = 0;
        for (int k = 0; k < NH_N; k++) {
            if (NH[k].node == n) { h = &NH[k]; break; }
            if (!fr && NH[k].tick + 2 <= t) fr = &NH[k];
        }
        float pd[3] = { 0, 0, 0 };
        if (h && h->tick + 1 == t) { pd[0] = h->d[0]; pd[1] = h->d[1]; pd[2] = h->d[2]; }
        if (!h) h = fr;
        if (h) { h->node = n; h->tick = t; h->d[0] = d[0]; h->d[1] = d[1]; h->d[2] = d[2]; }
        float d2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
        float p2 = pd[0] * pd[0] + pd[1] * pd[1] + pd[2] * pd[2];
        float dot = d[0] * pd[0] + d[1] * pd[1] + d[2] * pd[2];
        if (d2 == 0.0f) continue;
        if (!(dot > 0.0f && d2 <= 9.0f * p2 + 1.0f)) r->tick = t - 1;   /* veto this tick's blend */
    }
}
/* Node rotation (the 3x3 of the local matrix at node+0x4C, rows at +0x4C,
 * +0x5C, +0x6C; translation is column 3).  Logic sets it at 30 Hz; on logic
 * frames the midpoint rotation (row lerp + re-orthonormalisation, scale
 * kept) is written before AsdModelCalcProc and the true one is put back at
 * the end of the frame. */
#define NROW(n, i) ((volatile float *)((n) + 0x4C + (i) * 0x10))
static float r_len(const float *v) { return __builtin_sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
void rot_swap(u32 t)
{
    u32 hp = x3_heap();
    if (!hp) return;
    RHist *H = (RHist *)(hp + 16 + NH_N * sizeof(NHist));
    for (int i = 0; i < NREC; i++) {
        u32 n = S->rec[i].node;
        if (!n || S->rec[i].tick + 1 < t) continue;            /* set this tick (or vetoed this tick) */
        RHist *h = 0, *fr = 0;
        for (int k = 0; k < RH_N; k++) {
            if (H[k].node == n) { h = &H[k]; break; }
            if (!fr && H[k].tick + 2 <= t) fr = &H[k];
        }
        if (!h) { if (!fr) continue; h = fr; h->node = n; h->tick = 0; }
        if (h->tick == t || h->sw) continue;
        float c[9];
        for (int r = 0; r < 3; r++) for (int k = 0; k < 3; k++) c[r * 3 + k] = NROW(n, r)[k];
        int prev = h->tick + 1 == t, diff = 0, ok = prev;
        if (prev) for (int r = 0; r < 3; r++) {
            float *a = &h->R[r * 3], *b = &c[r * 3];
            float la = r_len(a), lb = r_len(b);
            if (!(la > 1e-4f && lb > 1e-4f)) { ok = 0; break; }
            float d = (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) / (la * lb);
            if (d < 0.7f) ok = 0;                                 /* > ~45 deg per tick: snap */
            float lr = la / lb;
            if (lr > 1.02f || lr < 0.98f) ok = 0;                  /* scale changed: snap */
            if (a[0] != b[0] || a[1] != b[1] || a[2] != b[2]) diff = 1;
        }
        if (ok && diff) {
            float m[9], len[3];
            for (int r = 0; r < 3; r++) {
                len[r] = r_len(&c[r * 3]);
                for (int k = 0; k < 3; k++) m[r * 3 + k] = (c[r * 3 + k] + h->R[r * 3 + k]) * 0.5f;
            }
            float *r0 = m, *r1 = m + 3, *r2 = m + 6;
            float l0 = r_len(r0);
            for (int k = 0; k < 3; k++) r0[k] /= l0;
            float d01 = r0[0] * r1[0] + r0[1] * r1[1] + r0[2] * r1[2];
            for (int k = 0; k < 3; k++) r1[k] -= d01 * r0[k];
            float l1 = r_len(r1);
            for (int k = 0; k < 3; k++) r1[k] /= l1;
            float x[3] = { r0[1] * r1[2] - r0[2] * r1[1], r0[2] * r1[0] - r0[0] * r1[2], r0[0] * r1[1] - r0[1] * r1[0] };
            float sg = (x[0] * r2[0] + x[1] * r2[1] + x[2] * r2[2]) < 0.0f ? -1.0f : 1.0f;
            for (int k = 0; k < 3; k++) r2[k] = x[k] * sg;
            for (int r = 0; r < 3; r++) for (int k = 0; k < 3; k++) {
                h->T[r * 3 + k] = c[r * 3 + k];
                NROW(n, r)[k] = m[r * 3 + k] * len[r];
            }
            *(volatile u32 *)(n + 0x88) |= 0x800u;
            h->sw = 1;
            ROT_ANY = 1;
        }
        for (int k = 0; k < 9; k++) h->R[k] = c[k];
        h->tick = t;
    }
}
void rot_restore(void)
{
    u32 hp = X3_HEAP;
    if (hp < 0x00100000u || *(u32 *)hp != X3_MAGIC) return;
    RHist *H = (RHist *)(hp + 16 + NH_N * sizeof(NHist));
    for (int k = 0; k < RH_N; k++) {
        RHist *h = &H[k];
        if (!h->sw) continue;
        for (int r = 0; r < 3; r++) for (int j = 0; j < 3; j++) NROW(h->node, r)[j] = h->T[r * 3 + j];
        *(volatile u32 *)(h->node + 0x88) |= 0x800u;
        h->sw = 0;
    }
}
#pragma clang section text=".ext.text" rodata=".ext.rodata"


/* ---------------------------------------------------------------------
 * Camera field of view.  The core blends camera pos/target/up, not the
 * projection.  Zoom tweens (Burroughs' bust-up scene camera: tween at
 * work+0x530 -> FUN_002C1FFC) change cam+0x28 (fov) at 30 Hz.  On
 * in-between frames the fov is recorded; on a logic frame a changed
 * fov is drawn at the midpoint (projection + stereo rebuilt) and put
 * back at run-end.  cam+0x28..0x34 = fov, aspect, near, far.
 * --------------------------------------------------------------------- */
#pragma clang section text=".ext4.text" rodata=".ext4.rodata"   /* needs the exheader (CAMPAGE) */
typedef struct { u32 tick, cam, sw; float last, tru; } FovRec;
#define FOV ((volatile FovRec *)0x004F0F8Cu)    /* 20 bytes, up to 0x4F0FA0 */
extern void CamSetProj_abs(void *cam, int rot, float fov, float aspect, float nr, float fr);
extern void CamUpdateStereo_abs(void *cam);
static void fov_proj(u32 cam, float f)
{
    volatile float *c = (volatile float *)cam;
    CamSetProj_abs((void *)cam, 1, f, c[11], c[12], c[13]);
    CamUpdateStereo_abs((void *)cam);
}
__attribute__((noinline)) static void fov_swap(void)
{
    u32 cam = (u32)GetGameCamera();
    if (cam < 0x00100000u) return;
    volatile FovRec *R = FOV;
    float f = *(volatile float *)(cam + 0x28);
    u32 t = tick_now() + 1;
    if (!S->logic) { R->tick = t; R->cam = cam; R->last = f; return; }
    if (R->tick + 1 != t || R->cam != cam) return;
    float d = f - R->last;
    if (d == 0.0f || d > 30.0f || d < -30.0f || !(f > 1.0f && f < 170.0f)) return;
    R->tru = f; R->sw = 1;
    fov_proj(cam, (f + R->last) * 0.5f);
}
__attribute__((noinline)) static void fov_restore(void)
{
    volatile FovRec *R = FOV;
    if (!R->sw) return;
    R->sw = 0;
    u32 cam = (u32)GetGameCamera();
    if (cam != R->cam) return;
    if (*(volatile float *)(cam + 0x28) != (R->tru + R->last) * 0.5f) return;   /* changed after the swap: keep */
    fov_proj(cam, R->tru);
}

/* Bust-up scene camera (BustupCutAnim, Burroughs' hologram): its own
 * camera at *(0x43DFC0)+4, moved by a 30 Hz tween (pos/target/up/fov).
 * Usually not the game camera while drawn, so the core never blends
 * it.  Same scheme: record on in-between frames, midpoint on logic
 * frames, true camera rebuilt at run-end. */
typedef struct { u32 tick, cam, sw; float last[10], tru[10]; } BCam;
#define BCAM ((volatile BCam *)0x004F1E80u)   /* 92 bytes, < 0x4F1F00 */
extern void CamSetLookAt_abs(void *cam, const float *pos, const float *tgt, const float *up);
static void bcam_apply(u32 cam, const float *v)
{
    volatile float *c = (volatile float *)cam;
    CamSetLookAt_abs((void *)cam, v, v + 3, v + 6);
    CamSetProj_abs((void *)cam, 1, v[9], c[11], c[12], c[13]);
    CamUpdateStereo_abs((void *)cam);
}
static void bcam_get(u32 cam, float *v)
{
    volatile float *c = (volatile float *)cam;
    for (int i = 0; i < 9; i++) v[i] = c[1 + i];
    v[9] = c[10];
}
__attribute__((noinline)) static void bcam_swap(void)
{
    u32 w = *(volatile u32 *)0x0043DFC0u;
    if (w < 0x00100000u) return;
    u32 cam = w + 4;
    if (cam == (u32)GetGameCamera()) return;          /* core handles the game camera */
    volatile BCam *R = BCAM;
    float v[10];
    bcam_get(cam, v);
    u32 t = tick_now() + 1;
    if (!S->logic) { R->tick = t; R->cam = cam; for (int i = 0; i < 10; i++) R->last[i] = v[i]; return; }
    if (R->tick + 1 != t || R->cam != cam) return;
    float m[10], d2 = 0.0f; int ch = 0;
    for (int i = 0; i < 10; i++) {
        float d = v[i] - R->last[i];
        if (d != 0.0f) ch = 1;
        if (i < 6) d2 += d * d;
        m[i] = (v[i] + R->last[i]) * 0.5f;
    }
    if (!ch || d2 > 300.0f * 300.0f) return;
    float df = v[9] - R->last[9];
    if (df > 30.0f || df < -30.0f) return;
    for (int i = 0; i < 10; i++) R->tru[i] = v[i];
    R->sw = 1;
    bcam_apply(cam, m);
}
__attribute__((noinline)) static void bcam_restore(void)
{
    volatile BCam *R = BCAM;
    if (!R->sw) return;
    R->sw = 0;
    float v[10];
    for (int i = 0; i < 10; i++) v[i] = R->tru[i];
    bcam_apply(R->cam, v);
}
#pragma clang section text=".ext.text" rodata=".ext.rodata"


/* ---------------------------------------------------------------------
 * Cameras moved outside the 30 Hz logic (e.g. the status-screen zoom on
 * the circle pad, run from a draw task every frame).  Blending those
 * makes the core put back a stale camera at the next frame start:
 * the zoom jitters.  Detect it and pause camera blending for 8 ticks:
 *  - in-between frame: the game camera differs from the core's record
 *    of the current tick (someone moved it without a logic tick);
 *  - run-end: the camera no longer holds the core's midpoint (someone
 *    moved it after the swap) -> keep their camera, cancel the restore.
 * --------------------------------------------------------------------- */
#define CEVERY (*(volatile u8 *)0x004F0FD0u)
static void cam_every_check(void)
{
    u32 cam = (u32)GetGameCamera();
    if (cam < 0x00100000u) return;
    volatile float *real = (volatile float *)cam;
    if (!S->logic) {
        if (S->camValid && S->camPtr == cam && !S->camSwapped)
            for (int i = 0; i < 9; i++) if (real[1 + i] != S->camCur[i]) { CEVERY = 8; break; }
        return;
    }
    if (CEVERY) { CEVERY--; S->camValid = 0; }
}
static void cam_post_check(void)
{
    if (!S->camSwapped) return;
    volatile float *real = (volatile float *)S->camPtr;
    for (int i = 0; i < 6; i++)                  /* pos + target (stored verbatim by SetLookAt) */
        if (real[1 + i] != (S->camPrev[i] + S->camCur[i]) * 0.5f) { S->camSwapped = 0; CEVERY = 8; return; }
}

void ext_watch_swap(SprTab *T, u32 t, u32 phase)
{
    if (phase == 0) {
        ext_install();
        spr_bigmove_fix(T, t);
        if (!CAMPAGE) CAMPAGE = *(volatile u32 *)0x004F1000u == 0x4D414343u ? 1 : 2;
        if (CAMPAGE == 1) { node_veto(t); rot_swap(t); }
        /* epl_swap(t);  particle interpolation off: flames looked wrong */
        if (CAMPAGE == 1) mif_swap(t);
        bg_swap(t);
        cam_every_check();
        if (CAMPAGE == 1 && !CEVERY) fov_swap();
    }
    for (int i = 0; i < NWATCH; i++) {
        const WatchDef *d = &WDEF[i];
        if (!d->n) break;
        if ((d->phase & 1) != phase) continue;
        int isint = d->phase & 2;                          /* n ints instead of floats */
        u32 base = d->ptr ? *(u32 *)d->ptr : 0;
        if (d->ptr && base < 0x00100000u) continue;
        float *v = (float *)(base + d->off);
        Watch *w = &T->w[i];
        if (w->tick == t && w->addr == (u32)v) continue;    /* already handled this frame */
        float *last = (float *)w->last, d2 = 0.0f;
        int *vi = (int *)v, *li = (int *)w->last;
        for (u32 k = 0; k < d->n; k++) { float e = isint ? (float)(vi[k] - li[k]) : v[k] - last[k]; d2 += e * e; }
        int ok = w->addr == (u32)v && w->tick == t - 1 && d2 > 0.0f && d2 <= THR2;
        w->swapped = ok ? d->n : 0;
        w->addr = (u32)v; w->tick = t;
        for (u32 k = 0; k < d->n; k++) {
            if (isint) {
                int c = vi[k];
                ((int *)w->cur)[k] = c;
                if (ok) vi[k] = li[k] + (c - li[k]) / 2;
                li[k] = c;
                continue;
            }
            float c = v[k];
            ((float *)w->cur)[k] = c;
            if (ok) v[k] = (c + last[k]) * 0.5f;
            last[k] = c;
        }
    }
}

void ext_watch_restore(SprTab *T)
{
    for (int i = 0; i < NWATCH; i++) {
        Watch *w = &T->w[i];
        for (u32 k = 0; k < w->swapped; k++) ((u32 *)w->addr)[k] = w->cur[k];
        w->swapped = 0;
    }
    epl_restore();
    epl_reset_corners();
    bg_restore();
    cam_post_check();
    if (CAMPAGE == 1) fov_restore();
    if (CAMPAGE == 1) mif_restore();
    if (ROT_ANY) { ROT_ANY = 0; rot_restore(); }
}
