/* Float parts of the interpolation module (ARM mode, VFP). */
#include "interp.h"
#define NODE_TX(n)    (*(volatile float *)((n) + 0x58))
#define NODE_TY(n)    (*(volatile float *)((n) + 0x68))
#define NODE_TZ(n)    (*(volatile float *)((n) + 0x78))
#define NODE_FLAGS(n) (*(volatile u32 *)((n) + 0x88))
#define THR2 (150.0f * 150.0f)
#ifdef REGION_EU      /* EUR cave (2400 bytes): the plugin's gate trampoline needs its last 8 bytes */
#define CAM_LIM THR2
#else
#define CAM_LIM (CAM_WIDE ? 3000.0f * 3000.0f : THR2)   /* attack zooms move further per tick than a cut test allows */
#endif

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
        if (i < 6) { if (d * d > CAM_LIM) ok = 0; if (d != 0.0f) moved = 1; }   /* cut -> no blend */
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

#ifdef REGION_APOC
/* Apocalypse h3d models (see hk_h3d_model): position at model+0x1B4, scale at
 * +0x1F0 (status screen zoom).  On a logic frame the update sees the midpoint
 * of the last two ticks; h3d_post puts the true values back.  Records live
 * after the State block (the Apocalypse .bss tail has 0xEE0 bytes before the
 * patch pages). */
typedef struct { u32 m, tick, sw; float p[3], s[3]; } HRec;    /* 36 bytes; sw: 1 position, 2 scale */
#define H3D_N 32
#define H3D   ((HRec *)((u32)S + 0x200))                 /* up to S + 0x680 */
#define FD    ((float *)((u32)S + 0x680))                /* screen fades: true alpha, top and bottom */
#define FD_SW (*(volatile u32 *)((u32)S + 0x688))
#define RL    ((volatile float *)((u32)S + 0x690))       /* relic scan: [0] counter fraction, [1] height scale, [2] tile size */
#define RL_C  (*(volatile int *)((u32)S + 0x69C))
#define RL_T  (*(volatile u32 *)((u32)S + 0x6A0))
#define H3D_POS(m) ((float *)((m) + 0x1B4))
#define H3D_SCL(m) ((float *)((m) + 0x1F0))

/* a zoom, not a model appearing or being reused: each axis moved, by at most half */
static inline int h3d_scl_near(const float *a, const float *b)
{
    int moved = 0;
    for (int k = 0; k < 3; k++) {
        float d = a[k] - b[k], m = a[k] > b[k] ? a[k] : b[k];
        if (d < 0.0f) d = -d;
        if (!(a[k] > 0.0f && b[k] > 0.0f) || d > m * 0.5f) return 0;
        if (d > 0.0f) moved = 1;
    }
    return moved;
}

/* Screen fades (hk_fade): alpha at +0 of two 0x14-byte records, recomputed
 * from a counter on each logic tick and only read by the draws.  A logic
 * frame shows the midpoint of the last two ticks, an in-between frame the
 * true value.  A jump (fade set directly) is not blended. */
extern void ScreenFadeUpdate(void);
extern float FadeRecs[];
void fade_step(void)
{
    float *A[2] = { FadeRecs, FadeRecs + 5 };
    if (!S->logic) {
        for (int i = 0; i < 2; i++) if (FD_SW & (1u << i)) *A[i] = FD[i];
        FD_SW = 0;
        return;
    }
    ScreenFadeUpdate();
    u32 sw = 0;
    for (int i = 0; i < 2; i++) {
        float c = *A[i], d = c - FD[i];
        if (d != 0.0f && d < 0.34f && d > -0.34f) { *A[i] = FD[i] + d * 0.5f; sw |= 1u << i; }
        FD[i] = c;
    }
    FD_SW = sw;
}

/* Relic scan tiles (draw 0x2DD40C, hk_relic_*): rise and fade come from the int
 * counter work+0x78 and the tile size from a table.  A logic frame draws the
 * counter half a step back and the midpoint of the last two sizes. */
float relic_begin(u32 w, float size)
{
    RL[0] = 0.0f; RL[1] = 1.0f;
    if (!S->logic) return size;
    u32 t = tick_now();
    int c = *(volatile int *)(w + 0x78), cont = RL_T + 1 == t;
    float p = RL[2];
    if (cont && c == RL_C + 1) { RL[0] = 0.5f; RL[1] = ((float)c - 0.5f) / (float)c; }
    RL_C = c; RL_T = t; RL[2] = size;
    return cont ? (size + p) * 0.5f : size;
}

extern void h3d_rot(u32 m, u32 post);                  /* extension (ext5), rotation +0x1C0 */
#define H3D_ROT_ON (S->emu && *(volatile u8 *)0x004F0FC4u == 1)   /* extension and its pages present */

void h3d_pre(u32 m)
{
    if (!S->logic || m < 0x08000000u) return;
    if (H3D_ROT_ON) h3d_rot(m, 0);
    u32 t = tick_now();
    HRec *r = 0, *fr = 0;
    for (int i = 0; i < H3D_N; i++) {
        HRec *q = &H3D[i];
        if (q->m == m) { r = q; break; }
        if (!fr && (q->m == 0 || q->tick + 1 < t)) fr = q;
    }
    if (!r) { if (!fr) return; r = fr; r->m = m; r->tick = 0; }
    r->sw = 0;
    if (r->tick == t) return;                           /* second update in this tick */
    float *P = H3D_POS(m), *Z = H3D_SCL(m);
    int ok = r->tick == t - 1 && near3(P, 1, r->p, 1);
    int okz = r->tick == t - 1 && h3d_scl_near(Z, r->s);
    for (int k = 0; k < 3; k++) {
        float c = P[k], z = Z[k];
        if (ok) P[k] = (c + r->p[k]) * 0.5f;
        if (okz) Z[k] = (z + r->s[k]) * 0.5f;
        r->p[k] = c; r->s[k] = z;                       /* true values */
    }
    r->sw = ok | okz << 1; r->tick = t;
}

void h3d_post(u32 m)
{
    if (!S->logic) return;
    if (H3D_ROT_ON && m >= 0x08000000u) h3d_rot(m, 1);
    for (int i = 0; i < H3D_N; i++) {
        HRec *r = &H3D[i];
        if (r->m != m) continue;
        if (r->sw & 1) { float *P = H3D_POS(m); for (int k = 0; k < 3; k++) P[k] = r->p[k]; }
        if (r->sw & 2) { float *Z = H3D_SCL(m); for (int k = 0; k < 3; k++) Z[k] = r->s[k]; }
        r->sw = 0;
        return;
    }
}
#endif

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
