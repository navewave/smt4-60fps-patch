/* Float parts of the interpolation module (ARM mode, VFP). */
#include "interp.h"
#define NODE_TX(n)    (*(volatile float *)((n) + 0x58))
#define NODE_TY(n)    (*(volatile float *)((n) + 0x68))
#define NODE_TZ(n)    (*(volatile float *)((n) + 0x78))
#define NODE_FLAGS(n) (*(volatile u32 *)((n) + 0x88))
#define THR2 (150.0f * 150.0f)

/* 1 if 0 < |a-b|^2 <= THR2 for 3-vectors with element strides sa/sb (in floats).
 * Inlined: saves 36 bytes of cave (EUR has only 2400). */
static inline int near3(const float *a, int sa, const float *b, int sb)
{
    float d2 = 0.0f;
    for (int k = 0; k < 3; k++) { float e = a[k * sa] - b[k * sb]; d2 += e * e; }
    return d2 > 0.0f && d2 <= THR2;
}

static void cam_swap(void)
{
    u32 cam = (u32)GetGameCamera();
    if (cam < 0x00100000u) return;
    float *real = (float *)(cam + 4);                 /* pos[3], target[3], up[3] */
    float *prev = S->camPrev;
    float mid[9];
    int ok = S->camValid && S->camPtr == cam, moved = 0;
    for (int i = 0; i < 9; i++) {
        float d = real[i] - prev[i];
        if (i < 6) { if (d * d > THR2) ok = 0; if (d != 0.0f) moved = 1; }   /* cut -> no blend */
        mid[i] = (real[i] + prev[i]) * 0.5f;
        S->camTrue[i] = real[i];
        prev[i] = real[i];
    }
    S->camValid = 1; S->camPtr = cam;
    if (ok && moved) { S->camSwapped = 1; cam_apply_bits(cam, (const u32 *)mid); }
}

void hk_modelcalc(void)
{
    /* Slots applied this frame carry tick == t, which rec_node() never
     * reassigns, so the true position can be parked in r->p itself. */
    u32 mask = 0;
    if (S->logic) {
        u32 t = tick_now();
        cam_swap();
        for (int i = 0; i < NREC; i++) {
            Rec *r = &S->rec[i];
            if (!r->node || r->tick != t) continue;
            float *T = (float *)(r->node + 0x58);          /* x @+0x58, y @+0x68, z @+0x78 */
            if (!near3(T, 4, r->p, 1)) continue;
            for (int k = 0; k < 3; k++) {
                float c = T[k * 4];
                T[k * 4] = (c + r->p[k]) * 0.5f;
                r->p[k] = c;                                   /* park true value */
            }
            NODE_FLAGS(r->node) |= 0x800u;
            mask |= 1u << i;
        }
    }
    ModelCalc();
    for (int i = 0; mask; i++, mask >>= 1) {
        if (!(mask & 1)) continue;
        Rec *r = &S->rec[i];
        float *T = (float *)(r->node + 0x58);
        for (int k = 0; k < 3; k++) T[k * 4] = r->p[k];
        NODE_FLAGS(r->node) |= 0x800u;
    }
}

static inline float f_of(u32 b) { union { u32 u; float f; } x; x.u = b; return x.f; }
static inline u32 b_of(float f) { union { u32 u; float f; } x; x.f = f; return x.u; }


void spr_swap(u32 phase)
{
    if (!S->logic) return;
    SprTab *T = spr_tab();
    if (!T) return;
    u32 t = tick_now();
    for (int i = 0; i < SPR_N; i++) {
        SRec *r = &T->e[i];
        if (!r->spr || r->tick != t || r->swapped) continue;
        u32 p = r->spr;
        int x = SPX(p), y = SPY(p); u32 a = SPA(p), b = SPB(p);
        int dx = x - r->px, dy = y - r->py;
        /* only blend what the animator produced (nothing changed it since) */
        int movePos = (dx | dy) && x == r->qx && y == r->qy
                                && (u32)(dx + SPR_POS_THR) < 2u * SPR_POS_THR
                                && (u32)(dy + SPR_POS_THR) < 2u * SPR_POS_THR;
        int moveA = a != r->pa && a == r->qa;
        /* scale track: +0x24, copied by the animator into scale X/Y +0x28/+0x2C */
        int moveB = b != r->pb && b == r->qb && SPA(p + 8) == b && SPA(p + 12) == b;
        if (!movePos && !moveA && !moveB) continue;
        r->cx = x; r->cy = y; r->ca = a; r->cb = b;
        r->swapped = moveB ? 3 : 1;
        if (movePos) { SPX(p) = r->px + (dx >> 1); SPY(p) = r->py + (dy >> 1); }
        if (moveA) SPA(p) = b_of((f_of(r->pa) + f_of(a)) * 0.5f);
        if (moveB) SPB(p) = SPA(p + 8) = SPA(p + 12) = b_of((f_of(r->pb) + f_of(b)) * 0.5f);
    }
    if (S->emu) ext_watch_swap(T, t, phase);

}
