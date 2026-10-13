/* Integer-only parts of the interpolation module, compiled as Thumb to save
 * cave space (ARMv6K Thumb-1 has no VFP, so no float arithmetic here;
 * float fields are moved as raw 32-bit words). */
#include "interp.h"

void rec_node(u32 node)
{
    if (!S->logic || node < 0x08000000u) return;
    u32 t = tick_now();
    int slot = -1;
    for (int i = 0; i < NREC; i++) {
        Rec *r = &S->rec[i];
        if (r->node == node) {
            if (r->tick != t) {
                r->tick = t;
                ((u32 *)r->p)[0] = *(volatile u32 *)(node + 0x58);
                ((u32 *)r->p)[1] = *(volatile u32 *)(node + 0x68);
                ((u32 *)r->p)[2] = *(volatile u32 *)(node + 0x78);
            }
            return;
        }
        if (slot < 0 && (r->node == 0 || r->tick != t)) slot = i;
    }
    if (slot < 0) return;
    Rec *r = &S->rec[slot];
    r->node = node; r->tick = t;
    ((u32 *)r->p)[0] = *(volatile u32 *)(node + 0x58);
    ((u32 *)r->p)[1] = *(volatile u32 *)(node + 0x68);
    ((u32 *)r->p)[2] = *(volatile u32 *)(node + 0x78);
}

void cam_apply_bits(u32 cam, const u32 *v)      /* v = pos[3], target[3], up[3] */
{
    CamSetLookAt((void *)cam, (const float *)v, (const float *)(v + 3), (const float *)(v + 6));
    CamUpdateStereo((void *)cam);
}

SprTab *spr_tab(void)
{
    SprTab *t = (SprTab *)S->heap;
    if (!t) return 0;
    if (t->magic != SPR_MAGIC) { S->heap = 0; S->heapDead = 1; return 0; }
    return t;
}

static void spr_put_true(SRec *r)
{
    u32 p = r->spr;
    SPX(p) = r->cx; SPY(p) = r->cy; SPA(p) = r->ca; SPB(p) = r->cb;
    if (r->swapped & 2)                       /* scale X/Y are copies of +0x24 */
        *(volatile u32 *)(p + 0x28) = *(volatile u32 *)(p + 0x2C) = r->cb;
    r->swapped = 0;
}

/* Find this tick's record for spr; if create, claim a free/stale slot. */
static SRec *spr_find(u32 spr, int create)
{
    if (!S->logic || spr < 0x00100000u) return 0;
    SprTab *T = spr_tab();
    if (!T) return 0;
    u32 t = tick_now();
    u32 h = ((spr >> 2) * 2654435761u) >> 24;
    SRec *freeSlot = 0;
    for (int k = 0; k < 32; k++) {
        SRec *r = &T->e[(h + k) & (SPR_N - 1)];
        if (r->spr == spr && r->tick == t) return r;
        if (!freeSlot && (r->spr == 0 || (r->tick != t && !r->swapped))) freeSlot = r;
    }
    if (!create || !freeSlot) return 0;
    freeSlot->spr = spr; freeSlot->tick = t; freeSlot->swapped = 4;   /* 4 = new */
    return freeSlot;
}

void spr_pre(u32 spr)
{
    SRec *r = spr_find(spr, 1);
    if (!r) return;
    if (r->swapped == 4) {                                   /* first update this tick */
        r->swapped = 0;
        r->qx = r->px = SPX(spr); r->qy = r->py = SPY(spr); r->qa = r->pa = SPA(spr); r->qb = r->pb = SPB(spr);
    } else if (r->swapped) spr_put_true(r);                  /* updated again during draw */
}

/* Animator exit: remember what the animator produced.  At swap time a
 * property is only blended if nothing else changed it afterwards. */
void spr_post(u32 spr)
{
    SRec *r = spr_find(spr, 0);
    if (r) { r->qx = SPX(spr); r->qy = SPY(spr); r->qa = SPA(spr); r->qb = SPB(spr); }
}

void spr_restore(void)
{
    SprTab *T = spr_tab();
    if (!T) return;
    for (int i = 0; i < SPR_N; i++)
        if (T->e[i].swapped) spr_put_true(&T->e[i]);
    if (S->emu) ext_watch_restore(T);
}

/* Azahar/Citra answer svcGetSystemInfo(0x20000 CITRA_INFORMATION, 0 EMULATOR_ID)
 * with 1 (Citra) / 2 (Azahar).  Real hardware (incl. Luma) returns an error.
 * SVC 0x2A is in this title's allowed-SVC list, so this is safe everywhere. */
static int is_emulator(void)
{
#ifdef NO_EMU_CHECK
    return 0;                      /* hardware test build: never issue svc 0x2A */
#endif
    register u32 r0 __asm__("r0");
    register u32 r1 __asm__("r1") = 0x20000;
    register u32 r2 __asm__("r2") = 0;
    __asm__ volatile("svc 0x2A" : "=r"(r0), "+r"(r1), "+r"(r2) : : "r3", "r12", "memory");
    return r0 == 0 && (r1 == 1 || r1 == 2);
}

void frame_c(void)
{
    if (!S->emuChecked) { S->emuChecked = 1; S->emu = is_emulator(); }
    if (!S->heap && !S->heapDead) {
        u32 p = GameAlloc(0x100, 0x100, 0, sizeof(SprTab), 1);  /* zero-filled */
        if (p >= 0x00100000u) { ((SprTab *)p)->magic = SPR_MAGIC; S->heap = p; }
        else S->heapDead = 1;
    }
    if (S->camSwapped) {
        S->camSwapped = 0;
        cam_apply_bits(S->camPtr, (const u32 *)S->camTrue);
    }
}
