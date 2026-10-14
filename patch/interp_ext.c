/* =====================================================================
 * Azahar-only extension.  Linked into the last bss page (0x4F0000-0x4F1000),
 * which Azahar's loader allocates before applying code.ips and which the
 * game's crt0 never clears (it stops at 0x4EFE34).  Azahar does not enforce
 * no-execute; a real 3DS does, so the core only calls in here when S->emu
 * was detected at boot.  Hooks into game code that need this region are
 * installed at runtime (ext_install) so the IPS stays harmless on hardware.
 * ===================================================================== */
#include "interp.h"

#define IN_BATTLE (*(volatile u8 *)0x004F0FF8u)   /* set at phase 0 (logic frames) */
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
/* 1 if this material member must keep whole frames: texture pattern
 * (type 7), int/bool tracks, or any curve segment with step
 * interpolation (flipbooks done by stepping UVs - torch flames).
 * Member: flags +0, component curves at +0xC + 4*i (self-relative;
 * flags bit i = constant value, bit ncomp+i = component absent).
 * Curve: flags +0xC (bit0 compound, bit2 single segment), count +0x10,
 * segment offsets +0x14..; segment flags +8: bit0 constant, bits 2-4
 * interpolation (0 = step, 1 = linear, 2 = hermite; FUN_00235794). */
#pragma clang section text=".ext.text" rodata=".ext.rodata"
static int off_ok(int o) { return o && o > -0x400000 && o < 0x400000; }
__attribute__((noinline)) static int mem_stepped(u32 m, float fsz)
{
    u32 ty = *(volatile u32 *)(m + 8), fl = *(volatile u32 *)m;
    int nc;
    switch (ty) {
    case 0: nc = 1; break;
    case 3: nc = 2; break;
    case 4: nc = 3; break;
    case 6: nc = 4; break;
    case 5: return 0;                          /* transform: not a material flipbook */
    default: return 1;                         /* 1 int, 2 bool, 7 texture, unknown */
    }
    for (int i = 0; i < nc; i++) {
        if (fl & (1u << i)) continue;                              /* constant */
        if (ty != 0 && (fl & (1u << (nc + i)))) continue;          /* absent */
        u32 sl = m + 0xC + 4 * i;
        int o = *(volatile int *)sl;
        if (!o) continue;
        if (!off_ok(o)) return 1;
        u32 c = sl + o, cf = *(volatile u32 *)(c + 0xC);
        if (cf & 1) return 1;                                      /* compound: be safe */
        if (cf & 2) continue;
        int ns = (cf & 4) ? 1 : *(volatile int *)(c + 0x10);
        if (ns < 1 || ns > 256) return 1;
        for (int j = 0; j < ns; j++) {
            u32 ss = (cf & 4) ? c + 0x14 : c + 0x14 + 4 * j;
            int so = *(volatile int *)ss;
            if (!off_ok(so)) return 1;
            u32 sg = ss + so, sf = *(volatile u32 *)(sg + 8);
            if (sf & 1) continue;                                  /* constant segment */
            if (((sf >> 2) & 7) == 0) return 1;                    /* step segment */
            /* frame-by-frame keys (flipbook baked as linear/hermite keys,
             * fire billboards): a key every <= 8 frames -> whole frames */
            float st = *(volatile float *)(sg + 4), en = fsz;
            if (j + 1 < ns && !(cf & 4)) {
                int no = *(volatile int *)(ss + 4);
                if (off_ok(no)) en = *(volatile float *)(ss + 4 + no + 4);
            }
            u32 kc = *(volatile unsigned short *)(sg + 0xC);
            float dur = en - st;
            if (kc >= 2 && dur > 0.0f && (float)kc * 8.0f >= dur) return 1;
        }
    }
    return 0;
}
#pragma clang section text=".ext4.text" rodata=".ext4.rodata"   /* needs the exheader (CAMPAGE) */
/* Material animation: whole frames only if it swaps textures (flipbook:
 * member primitive type 7, see the evaluator FUN_003E8E64).  Pure UV
 * scroll / colour / alpha animations (heat haze, glows) stay smooth.
 * ResAnim +0x18 member count, +0x1C dict; member i data =
 * e + *e with e = dict + 0x28 + i*0x10, type at data+8.  Cached per
 * ResAnim in the DF heap block (unknown / not ready -> whole frames). */
#define MC_N 128
#define DF_HEAP_P (*(volatile u32 *)0x004F0FF0u)
static u32 mat_pattern(u32 res)
{
    u32 hp = DF_HEAP_P;
    if (hp < 0x00100000u || *(volatile u32 *)hp != 0x44465231u) return 1;
    volatile u32 *C = (volatile u32 *)(hp + 16 + 33 * 16);
    u32 h = ((res >> 2) & (MC_N - 1)) * 2;
    if (!heap_ptr(res)) return 1;
    int cnt = *(volatile int *)(res + 0x18), o = *(volatile int *)(res + 0x1C);
    if (C[h] == res && (C[h + 1] >> 1) == (u32)cnt) return C[h + 1] & 1;   /* same resource still there */
    u32 cls = 1;
    {
        if (cnt > 0 && cnt <= 256 && o && o > -0x400000 && o < 0x400000) {
            u32 dict = res + 0x1C + o;
            cls = 0;
            for (int i = 0; i < cnt && !cls; i++) {
                u32 e = dict + 0x28 + i * 0x10;
                int eo = *(volatile int *)e;
                if (!eo || eo < -0x400000 || eo > 0x400000) { cls = 1; break; }
                cls = mem_stepped(e + eo, *(volatile float *)(res + 0x14));
            }
        }
    }
    C[h] = res; C[h + 1] = cls | ((u32)cnt << 1);
    return cls;
}
u32 updf_class(u32 anim)
{
    u32 g = *(volatile u32 *)(anim + 8);
    u32 n1 = heap_ptr(g) ? name2(bin_str(*(volatile u32 *)(g + 8), 8)) : 0;
    u32 n2 = name2(bin_str(*(volatile u32 *)(anim + 0x54), 0x0C));
#if UPDF_MODE == 3
    if (n1 == N2('V','i') || n2 == N2('V','i')) return 1;
    /* materials: whole frames, except in battle (heat haze, glows), where
     * only flipbook-like ones (stepped / densely keyed curves, texture
     * swaps) keep whole frames.  Field fire billboards defeat that test. */
    if (n1 == N2('M','a') || n2 == N2('M','a'))
        return IN_BATTLE ? mat_pattern(*(volatile u32 *)(anim + 0x54)) : 1;
    return 0;
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
 * FUN_001AA5A0 2D).  Particle positions are not interpolated (flames keep
 * their original look); only the in-place corner transform is undone each
 * frame (epl_reset_corners).  The per-particle record hook of 5a-5t was
 * removed in 5y (it only fed the disabled epl_swap).
 * --------------------------------------------------------------------- */
#define EPL_UPDATE   0x001929BCu
#define EFF_LISTS    0x004C5830u        /* 5 lists x {head, tail, count, -} */
#define W(d, o)  (*(volatile u32 *)((d) + (o)))

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

/* ---------------------------------------------------------------------
 * Runtime hook installation (Azahar only).  svc 0x93 is
 * InvalidateInstructionCacheRange, which Azahar forwards to the JIT.
 * --------------------------------------------------------------------- */

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
#define QUAD_ORIGWORD  0xEE305A01u        /* vadd.f32 s10, s0, s2 */
#define Q_CNT   256
#define Q_REC   1024
#define Q_MAGIC 0x51554144u
#define Q_MAXD  192.0f
typedef struct { u32 base, stamp, n; } QCnt;
typedef struct { u32 key, tick; float x, y, dx, dy; } QRec;
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
        if (r->tick + 1 == t) { r->dx = x - r->x; r->dy = y - r->y; }
        else { r->dx = r->dy = 0.0f; }
        r->x = x; r->y = y; r->tick = t;
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
#ifdef QDIAG
"    mov   r0, sp\n"
"    ldr   r12, =0x004F0FC4\n"
"    ldrb  r12, [r12]\n"
"    cmp   r12, #1\n"
"    bleq  quad_detect\n"
#endif
"    vpop  {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    pop   {r0, r1, r2, r3, r12, lr}\n"
"    vadd.f32 s10, s0, s2\n"
"    b     QuadFn_cont\n"
"    .ltorg\n"
);



__asm__(
"    .section .ext.text.scl,\"ax\",%progbits\n"
"    .arm\n"
"    .global ext_setscale\n"
"ext_setscale:\n"                      /* entered by b from 0x25C0F0 */
"    push  {r0, r1, r2, r3, r12, lr}\n"
"    vpush {d0, d1}\n"
"    bl    scl_record\n"
"    vpop  {d0, d1}\n"
"    pop   {r0, r1, r2, r3, r12, lr}\n"
"    ldr   r0, [r0, #0x2c]\n"
"    b     SetScale_cont\n"
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
    {   /* anim step: replace the core's "b hk_updframe" */
        extern char hk_updframe[], ext_updframe[];
        u32 w = *(volatile u32 *)UPDF_SITE;
        if (UPDF_MODE && CAMPAGE == 1 && (w >> 24) == 0xEAu && UPDF_SITE + 8 + ((int)(w << 8) >> 6) == (u32)hk_updframe) {
            put_branch(UPDF_SITE, (u32)ext_updframe); icache_inval(UPDF_SITE, 4);
        }
    }
    if (CAMPAGE == 1 && *(volatile u32 *)0x0025C0F0u == 0xE590002Cu) {   /* SetScale: ldr r0,[r0,#0x2c] */
        extern char ext_setscale[];
        put_branch(0x0025C0F0u, (u32)ext_setscale); icache_inval(0x0025C0F0u, 4);
    }
    if (*(volatile u32 *)QUAD_FN == QUAD_ORIGWORD) { put_branch(QUAD_FN, (u32)ext_quad); icache_inval(QUAD_FN, 4); }
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
typedef struct { u32 tick, cam, sw; float last, tru, mid; } FovRec;
#define FOV ((volatile FovRec *)0x004F0FD8u)    /* 24 bytes, up to 0x4F0FF0 */
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
    /* phase 0 only runs on logic frames: the previous tick's fov is the
     * one recorded here one tick ago (true values are back by then) */
    u32 cam = (u32)GetGameCamera();
    if (cam < 0x00100000u) return;
    volatile FovRec *R = FOV;
    float f = *(volatile float *)(cam + 0x28), p = R->last;
    u32 t = tick_now() + 1;
    int cont = R->tick + 1 == t && R->cam == cam;
    R->tick = t; R->cam = cam; R->last = f;
    float d = f - p;
    if (!cont || d == 0.0f || d > 30.0f || d < -30.0f || !(f > 1.0f && f < 170.0f)) return;
    R->tru = f; R->mid = (f + p) * 0.5f; R->sw = 1;
    fov_proj(cam, R->mid);
}
__attribute__((noinline)) static void fov_restore(void)
{
    volatile FovRec *R = FOV;
    if (!R->sw) return;
    R->sw = 0;
    u32 cam = (u32)GetGameCamera();
    if (cam != R->cam) return;
    if (*(volatile float *)(cam + 0x28) != R->mid) return;   /* changed after the swap: keep */
    fov_proj(cam, R->tru);
}

/* ---------------------------------------------------------------------
 * Diorama cameras (area select, event backdrops).  While scene flag bit i
 * (*(0x464AF8)) is set, scene i's NW4C camera (*(0x464B10 + i*0xC)) is not
 * synced from the game camera; FUN_00341C74 (30 Hz calc) tweens it: node
 * translation +0x58/+0x68/+0x78 and the view updater's rotation vector
 * upd+8..+0x10 (FUN_0023D51C).  GDB showed no other writer and writes on
 * logic frames only.  Midpoint swap on logic frames, true values at run-end.
 * --------------------------------------------------------------------- */
#pragma clang section text=".ext3.text" rodata=".ext3.rodata"
typedef struct { u32 cam, upd, tick, sw; float last[6], tru[6]; } DcRec;
#define DC ((DcRec *)0x004F1E80u)                /* 2 * 64 bytes, up to the camera state at 0x4F1F00 */
#define SCN_FLAGS (*(volatile u32 *)0x00464AF8u)
static void dc_ptrs(const DcRec *r, float **p)
{
    for (int k = 0; k < 3; k++) { p[k] = (float *)(r->cam + 0x58 + k * 0x10); p[k + 3] = (float *)(r->upd + 8 + k * 4); }
}
__attribute__((noinline)) static void dc_restore(void)
{
    for (DcRec *r = DC; r < DC + 2; r++) {
        if (!r->sw) continue;
        r->sw = 0;
        float *p[6]; dc_ptrs(r, p);
        for (int k = 0; k < 6; k++) *p[k] = r->tru[k];
        NODE_FLAGS(r->cam) |= FLAG_LOCAL_DIRTY;
    }
}
#pragma clang section text=".ext5.text" rodata=".ext5.rodata"
__attribute__((noinline)) static void dc_swap(u32 t)
{
    for (int i = 0; i < 2; i++) {
        DcRec *r = &DC[i];
        u32 cam = *(volatile u32 *)(0x00464B10u + i * 0xC), o, upd = 0;
        if ((SCN_FLAGS & (1u << i)) && cam >= 0x00100000u && (o = *(u32 *)(cam + 0x258)) >= 0x00100000u)
            upd = ((u32 (*)(u32))*(u32 *)(*(u32 *)o + 0x10))(o);      /* view updater (FUN_0023D51C) */
        if (upd < 0x00100000u || !(*(u32 *)upd & 0x20000000u)) { r->cam = 0; continue; }
        int ok = r->cam == cam && r->upd == upd && r->tick == t - 1, ch = 0;
        r->cam = cam; r->upd = upd; r->tick = t;
        float *p[6]; dc_ptrs(r, p);
        for (int k = 0; k < 6; k++) {
            float c = *p[k], d = c - r->last[k];
            if (d != 0.0f) ch = 1;
            if (d * d > THR2) ok = 0;                 /* cut, or angle wrap */
            r->tru[k] = c;
            *p[k] = (c + r->last[k]) * 0.5f;          /* undone below unless blending */
            r->last[k] = c;
        }
        r->sw = ok && ch;
        if (!r->sw) for (int k = 0; k < 6; k++) *p[k] = r->tru[k];
        NODE_FLAGS(cam) |= FLAG_LOCAL_DIRTY;
    }
}

/* ---------------------------------------------------------------------
 * Event portraits.  Their logic tasks (prio 2) step plain fields of the
 * task work at 30 Hz and their draw tasks read them every frame:
 *  - "EVE_2dTex" children, calc FUN_001DFB60 (talk bust-ups, Burroughs):
 *    +0x10 int slide x, +0x1C decaying offset, +0x24 alpha (zoom scale
 *    when +0x64 is set); drawn by FUN_002857F4.
 *  - "EVE_BustupTex", calc FUN_001CE3A4 (BustupCutAnim scenes): +0x34..
 *    +0x3C centre, +0x58 x offset, +0x1C/+0x20 half size, +0x28/+0x88
 *    alpha; drawn by FUN_001DBB54.
 * The prio-2 task list is walked on logic frames; listed fields are drawn
 * at the midpoint of the last two ticks and put back at run-end.  A tick
 * that changes a portrait's state words (texture, mode, crossfade flag)
 * is not blended: an expression change starts its crossfade by jumping the
 * old face's alpha +0x88 from 0 to 1 together with the texture swap.
 * --------------------------------------------------------------------- */
#pragma clang section text=".ext5.text" rodata=".ext5.rodata"   /* third extra page: needs the exheader */
#define PT_NF 8
#define PT_N  12
typedef struct { u32 w, tick, sw, sig; u32 last[PT_NF], tru[PT_NF]; } PtRec;   /* 80 bytes */
#define PT ((PtRec *)0x004F3C00u)                /* 12 * 80 bytes, ext5 data */
typedef struct { u32 fn; u8 nint, n, off[PT_NF], g[4]; } PtDef;   /* first nint fields are ints; g = state words */
static const PtDef PT_DEF[2] = {
    { 0x001DFB60u, 1, 3, { 0x10, 0x1C, 0x24 }, { 0x0C, 0x3C, 0x40, 0x48 } },
    { 0x001CE3A4u, 0, 8, { 0x34, 0x38, 0x3C, 0x58, 0x1C, 0x20, 0x28, 0x88 }, { 0x0C, 0x24, 0x80, 0x8C } },
};
__attribute__((noinline)) static void pt_swap(u32 t)
{
    PtRec *r = PT;
    for (u32 k = *(volatile u32 *)(*(volatile u32 *)0x004E3CF4u + 2 * 8); k && r < PT + PT_N; k = *(u32 *)(k + 0x4C)) {
        u32 fn = *(u32 *)(k + 0x3C), w = *(u32 *)(k + 0x48);
        const PtDef *d = fn == PT_DEF[0].fn ? &PT_DEF[0] : fn == PT_DEF[1].fn ? &PT_DEF[1] : 0;
        if (!d || w < 0x00100000u) continue;
        u32 sig = 0;
        for (int g = 0; g < 4; g++) sig = (sig << 7 | sig >> 25) ^ *(u32 *)(w + d->g[g]);
        int ok = r->w == w && r->tick == t - 1 && r->sig == sig, ch = 0;
        r->w = w; r->tick = t; r->sw = 0; r->sig = sig;
        u32 m[PT_NF];
        for (int f = 0; f < d->n; f++) {
            u32 c = *(u32 *)(w + d->off[f]), l = r->last[f];
            r->tru[f] = c; r->last[f] = c; m[f] = c;
            if (c == l) continue;
            ch = 1;
            if (f < d->nint) {
                int dd = (int)c - (int)l;
                if (dd > 400 || dd < -400) ok = 0;
                m[f] = (u32)((int)l + dd / 2);
            } else {
                float dd = f_of(c) - f_of(l);
                if (dd * dd > THR2) ok = 0;
                m[f] = b_of(f_of(l) + dd * 0.5f);
            }
        }
        if (ok && ch) {
            for (int f = 0; f < d->n; f++) *(u32 *)(w + d->off[f]) = m[f];
            r->sw = d->n;
        }
        r++;
    }
    for (; r < PT + PT_N; r++) r->w = 0;
}
__attribute__((noinline)) static void pt_restore(void)
{
    for (PtRec *r = PT; r < PT + PT_N; r++) {
        if (!r->sw) continue;
        const PtDef *d = r->sw == 3 ? &PT_DEF[0] : &PT_DEF[1];
        for (int f = 0; f < d->n; f++) *(u32 *)(r->w + d->off[f]) = r->tru[f];
        r->sw = 0;
    }
}

#ifdef QDIAG
/* ---------------------------------------------------------------------
 * 30 Hz detector (diagnostic builds only, -DQDIAG).  Called from ext_quad
 * after quad_interp with the saved frame: s0..s15, r0..r3 (vertex colours),
 * r12, lr.  Tracks every 2D quad by texture + uv + occurrence and compares
 * what is drawn (after smoothing) frame to frame.  A quad that changes on
 * logic frames but not on the in-between frames for 3 ticks in a row is
 * reported as {task fn, caller lr, its caller lr2 when lr is a rect-draw
 * wrapper, texture, last x/y, task prio, mask}: mask 1 = position,
 * 2 = size, 4 = colour/alpha; n = number of such runs.  Table on the game
 * heap, pointer at 0x4F3FC0 (magic 'QD01').  Read with tools/gdb/qd_dump.py.
 * --------------------------------------------------------------------- */
#pragma clang section text=".ext5.text" rodata=".ext5.rodata"
#define QD_N 512
#define QD_R 160
#define QD_MAGIC 0x31304451u
typedef struct { u32 key, frame; u32 v[8]; u8 chL, streak, pad[2]; } QdRec;   /* 44 bytes */
typedef struct { u32 task, lr, lr2, tex, x, y; u16 prio; u8 mask, pad; u16 n, pad2; } QdRep;   /* 32 bytes */
typedef struct { u32 magic, hits, lost, pad; QCnt c[64]; QdRec r[QD_N]; QdRep rep[QD_R]; } QdTab;
#define QD_HEAP (*(volatile u32 *)0x004F3FC0u)
void quad_detect(u32 *f)
{
    u32 hp = QD_HEAP;
    if (hp == 1) return;
    if (hp < 0x00100000u || *(u32 *)hp != QD_MAGIC) {
        hp = GameAlloc_abs(0x100, 0x100, 0, sizeof(QdTab), 1);
        if (hp < 0x00100000u) { QD_HEAP = 1; return; }
        for (u32 i = 0; i < sizeof(QdTab); i += 4) *(u32 *)(hp + i) = 0;
        *(u32 *)hp = QD_MAGIC;
        QD_HEAP = hp;
    }
    QdTab *T = (QdTab *)hp;
    u32 tex = *(volatile u32 *)(*(volatile u32 *)0x0044EA00u + 0x11E4);
    u32 base = (tex * 0x9E3779B1u ^ f[4] * 0x27D4EB2Fu ^ f[5] * 0x165667B1u ^ f[6] * 0x85EBCA77u ^ f[7] * 0xC2B2AE3Du) | 1;
    u32 fr = S->counter, occ = 0;
    QCnt *c = &T->c[base >> 26];
    if (c->stamp != fr) { c->stamp = fr; c->base = base; c->n = 1; }
    else if (c->base == base) occ = c->n++;
    u32 key = base + occ * 0x61C88647u;
    QdRec *r = &T->r[(key >> 23) & (QD_N - 1)];
    u32 v[8] = { f[0], f[1], f[2], f[3], f[16], f[17], f[18], f[19] };
    if (r->key != key) {
        r->key = key; r->frame = fr; r->chL = r->streak = 0;
        for (int k = 0; k < 8; k++) r->v[k] = v[k];
        return;
    }
    if (r->frame == fr) return;                        /* second draw this frame (other eye) */
    u32 mask = 0;
    if (r->frame + 1 == fr) {
        if (v[0] != r->v[0] || v[1] != r->v[1]) mask |= 1;
        if (v[2] != r->v[2] || v[3] != r->v[3]) mask |= 2;
        for (int k = 4; k < 8; k++) if (v[k] != r->v[k]) mask |= 4;
    } else r->chL = r->streak = 0;
    if (S->logic) r->chL = mask;
    else {
        if (r->chL && !mask) {
            if (++r->streak == 3) {                    /* 3 ticks changing at 30 Hz: report */
                u32 cur = *(volatile u32 *)0x004E3CFCu;
                u32 task = cur >= 0x00100000u ? *(u32 *)(cur + 0x3C) : 0, lr = f[21];
                /* rect-draw wrappers: their saved lr above our 22-word frame */
                u32 lr2 = lr == 0x002C1D2Cu ? f[22 + 15] : lr == 0x002B7A94u ? f[22 + 11] : 0;
                QdRep *e = 0;
                for (int i = 0; i < QD_R; i++) {
                    QdRep *q = &T->rep[i];
                    if (q->n && q->task == task && q->lr == lr && q->lr2 == lr2) { e = q; break; }
                    if (!q->n) { e = q; e->task = task; e->lr = lr; e->lr2 = lr2; e->tex = tex; e->prio = cur >= 0x00100000u ? *(u32 *)(cur + 0x20) : 0; break; }
                }
                if (e) { e->mask |= r->chL; e->x = v[0]; e->y = v[1]; if (e->n != 0xFFFF) e->n++; T->hits++; }
                else T->lost++;
            }
        } else r->streak = 0;
        r->chL = 0;
    }
    r->frame = fr;
    for (int k = 0; k < 8; k++) r->v[k] = v[k];
}
#endif

/* Material classification cache (mat_pattern): 128 x {res, cls|cnt<<1}
 * on the game heap; pointer at 0x4F0FF0 (magic 'DFR1', layout kept). */
#pragma clang section text=".ext.text" rodata=".ext.rodata"
__attribute__((noinline)) static void mc_alloc(void)
{
    u32 hp = *(volatile u32 *)0x004F0FF0u;
    if (hp == 1 || (hp >= 0x00100000u && *(volatile u32 *)hp == 0x44465231u)) return;
    u32 sz = 16 + 33 * 16 + MC_N * 8;
    hp = GameAlloc_abs(0x100, 0x100, 0, sz, 1);
    if (hp < 0x00100000u) { *(volatile u32 *)0x004F0FF0u = 1; return; }
    for (u32 i = 0; i < sz; i += 4) *(volatile u32 *)(hp + i) = 0;
    *(volatile u32 *)hp = 0x44465231u;
    *(volatile u32 *)0x004F0FF0u = hp;
}
#pragma clang section text=".ext4.text" rodata=".ext4.rodata"
/* ---------------------------------------------------------------------
 * Model scale (SetScale FUN_0025C0F0: node=*(m+0x2C), scale node+0x7C..
 * +0x84, flag 0x800).  The status / load screen zoom scales the model
 * and moves the camera each tick; the core blends the camera only, so
 * on logic frames camera and model disagree (zoom jitters).  Scale
 * writes are recorded (value before the tick's first write) and drawn
 * at the midpoint on logic frames, true scale back at run-end.
 * --------------------------------------------------------------------- */
#define SCL_N 12
typedef struct { u32 node, tick, sw; float pre[3], tru[3]; } SclRec;
#define SCL ((volatile SclRec *)0x004F2E00u)          /* 12 * 36 bytes */
#define SCL_ANY (*(volatile u32 *)0x004F2FF0u)
void scl_record(u32 m)
{
    if (!S->logic) return;
    u32 n = *(volatile u32 *)(m + 0x2C);
    if (n < 0x00100000u) return;
    u32 t = tick_now() + 1;
    volatile SclRec *fr = 0;
    for (int i = 0; i < SCL_N; i++) {
        volatile SclRec *r = &SCL[i];
        if (r->node == n) { if (r->tick == t) return; fr = r; break; }
        if (!fr && r->tick != t) fr = r;
    }
    if (!fr) return;
    volatile float *sc = (volatile float *)(n + 0x7C);
    fr->node = n; fr->tick = t; fr->sw = 0;
    for (int k = 0; k < 3; k++) fr->pre[k] = sc[k];
    SCL_ANY = 1;
}
__attribute__((noinline)) static void scl_swap(void)
{
    if (!S->logic || !SCL_ANY) return;
    u32 t = tick_now() + 1;
    for (int i = 0; i < SCL_N; i++) {
        volatile SclRec *r = &SCL[i];
        if (r->tick != t || !r->node) continue;
        volatile float *sc = (volatile float *)(r->node + 0x7C);
        float m[3]; int ok = 1, ch = 0;
        for (int k = 0; k < 3; k++) {
            float c = sc[k], p = r->pre[k];
            if (c != p) ch = 1;
            if (!(p > 0.0f) || c > p * 1.5f || c < p * 0.66f) ok = 0;
            m[k] = (c + p) * 0.5f;
        }
        if (!ch || !ok) continue;
        for (int k = 0; k < 3; k++) { r->tru[k] = sc[k]; sc[k] = m[k]; }
        NODE_FLAGS(r->node) |= FLAG_LOCAL_DIRTY;
        r->sw = 1;
    }
}
__attribute__((noinline)) static void scl_restore(void)
{
    if (!SCL_ANY) return;
    for (int i = 0; i < SCL_N; i++) {
        volatile SclRec *r = &SCL[i];
        if (!r->sw) continue;
        r->sw = 0;
        volatile float *sc = (volatile float *)(r->node + 0x7C);
        for (int k = 0; k < 3; k++) sc[k] = r->tru[k];
        NODE_FLAGS(r->node) |= FLAG_LOCAL_DIRTY;
    }
}


#pragma clang section text=".ext.text" rodata=".ext.rodata"


/* Battle active: battle work *(0x435D90), task handle at +0, "BTLMAIN C"
 * alive (FUN_002BF41C) and not being torn down (+0x60). */
extern u32 TaskAlive(u32 handle);                  /* FUN_002BF41C */
static void battle_check(void)
{
    u32 w = *(volatile u32 *)0x00435D90u, on = 0;
    if (w >= 0x08000000u && w < 0x40000000u && !(w & 3)) {
        u32 h = *(volatile u32 *)w;
        if (h && (h >> 20) <= 0x10 && !*(volatile u8 *)(w + 0x60)) on = TaskAlive(h) != 0;
    }
    IN_BATTLE = on;
}
void ext_watch_swap(SprTab *T, u32 t, u32 phase)
{
    if (phase == 0) {
        ext_install();
        spr_bigmove_fix(T, t);
        if (!CAMPAGE) CAMPAGE = *(volatile u32 *)0x004F1000u == 0x4D414343u ? 1 : 2;
        if (CAMPAGE == 1) { node_veto(t); rot_swap(t); }
        if (CAMPAGE == 1) mif_swap(t);
        bg_swap(t);
        if (CAMPAGE == 1) { mc_alloc(); battle_check(); fov_swap(); scl_swap(); dc_swap(t); pt_swap(t); }
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
    epl_reset_corners();
    bg_restore();
    if (CAMPAGE == 1) { scl_restore(); fov_restore(); dc_restore(); pt_restore(); }
    if (CAMPAGE == 1) mif_restore();
    if (ROT_ANY) { ROT_ANY = 0; rot_restore(); }
}
