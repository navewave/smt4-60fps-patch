/* =====================================================================
 * SMT IV (USA, 00040000000E5C00) - right stick / C-stick camera
 *
 * Azahar-only add-on.  Lives in its own page 0x4F1000-0x4F2000, which exists
 * because the patched exheader (mods/<tid>/exheader.bin) raises the bss size by
 * 0x1000; the 60 fps patch only uses 0x4EFE38-0x4F1000, so the two coexist.
 * A real 3DS maps this page no-execute.
 *
 * Field camera (FUN_00264368, called from the MAP state machine at 30 Hz):
 *   cam = *(0x4474F4); yaw in degrees at cam+0x18.  L held: +3 deg/tick,
 *   R held: -3 deg/tick, L+R: snap behind the player (15 deg/tick).  The yaw
 *   is wrapped to [0,360) and copied to map+0x50, which both the camera
 *   placement and the player's walk direction use, so turning the camera
 *   here keeps movement camera-relative exactly like L/R do.
 * Camera placement FUN_0026617C(map):
 *   cam pos    map+0x60/0x64/0x68 = player - (sin,cos)(yaw)*dist, y = player+h
 *   look-at    map+0x6C/0x70/0x74 = player (+ target height)
 *   There is no pitch; cam_stick_pitch() rotates the (horizontal, vertical)
 *   offset of the camera around the look-at point.  Collision, easing and
 *   smoothing in the rest of the function then work on the rotated position.
 *
 * C-stick: the game never opens ir:rst (it is not even in its exheader
 * service list; Azahar does not enforce that list).  The patch opens it with
 * the game's own srv wrapper, maps the shared memory with the game's own
 * shared-memory helper (so the address cannot collide with the game's
 * mappings) and reads the newest pad entry every logic tick.
 * ===================================================================== */
typedef unsigned int   u32;
typedef unsigned short u16;
typedef short          s16;
typedef unsigned char  u8;

#pragma clang section text=".cam.text" rodata=".cam.rodata"

/* ---- layout of the page --------------------------------------------------
 *   0x4F1000  magic 'CCAM' (the cheat only installs its hooks if it reads back)
 *   0x4F1004  Cfg, written by its own cheat lines (editable without rebuilding)
 *   0x4F1020  code
 *   0x4F1F00  runtime state (never written by the cheat)
 * ------------------------------------------------------------------------ */
__attribute__((section(".cam.magic"), used)) const u32 cam_magic = 0x4D414343u;
typedef struct {
    float yawSpeed;     /* degrees per logic tick at full deflection (L/R = 3)  */
    float pitchSpeed;   /* radians per logic tick at full deflection            */
    float pitchMin;     /* radians, negative = camera lower (looking up)        */
    float pitchMax;     /* radians, positive = camera higher (looking down)     */
    u32   flags;        /* bit0 invert X, bit1 invert Y, bit2 L+R resets pitch, bit3 debug: slow spin if the stick could not be opened */
} Cfg;
__attribute__((section(".cam.cfg"), used))
#ifndef CAM_FLAGS
#define CAM_FLAGS 4
#endif
const volatile Cfg cam_cfg = { 6.0f, 0.0525f, -0.35f, 0.60f, CAM_FLAGS };

/* ---- runtime state ------------------------------------------------------ */
typedef struct {
    u32   blk[7];       /* nn::os shared-memory block: next, prev, addr, size, ro, handle, mapped */
    u32   state;        /* 0 = not tried, 1 = running, 2 = ir:rst unavailable */
    float pitch;        /* current pitch offset in radians */
    u32   rst;          /* ir:rst session handle */
} CamState;
#ifndef CAM_STATE
#define CAM_STATE 0x004F1F00u
#endif
#define C ((volatile CamState *)CAM_STATE)

/* game functions */
extern int  SrvGetServiceHandle(u32 *out, const char *name, u32 len, u32 flags); /* FUN_0012954C */
extern int  ShmBlockMap(volatile u32 *blk, u32 size, u32 readonly);             /* FUN_0013903C */

#define CAM_PTR    0x004474F4u     /* field camera controller object */
#define MAPFLAGS   0x0049F91Cu     /* +0x18 u8, +0x1C u32: camera input locked (events, menus) */

static inline volatile u32 *cmdbuf(void)
{
    u32 *tls;
    __asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(tls));
    return (volatile u32 *)(tls + 0x20);
}

static int send_sync(u32 h)
{
    register u32 r0 __asm__("r0") = h;
    __asm__ volatile("svc 0x32" : "+r"(r0) : : "r1", "r2", "r3", "r12", "memory");
    return (int)r0;
}

static void stick_init(void)
{
    C->state = 2;
    /* the srv wrapper copies the name as two words: 8 bytes, word aligned */
    static const char name[8] __attribute__((aligned(4))) = "ir:rst";
    u32 h = 0;
    if (SrvGetServiceHandle(&h, name, 6, 0) < 0 || !h) return;
    volatile u32 *cb = cmdbuf();
    cb[0] = 0x00010000;                          /* GetHandles */
    if (send_sync(h) < 0 || (int)cb[1] < 0) return;
    u32 shm = cb[3];
    cb[0] = 0x00020080;                          /* Initialize(update period ms, raw c-stick) */
    cb[1] = 10;
    cb[2] = 0;
    if (send_sync(h) < 0) return;                /* "already initialised" is fine */
    for (int i = 0; i < 7; i++) C->blk[i] = 0;
    C->blk[5] = shm;
    if (ShmBlockMap(C->blk, 0x1000, 1) < 0 || C->blk[2] < 0x00100000u) return;
    C->rst = h;
    C->state = 1;
}

/* stick axis -> [-1, 1] with dead zone and a quadratic response */
static float axis(int v)
{
    float f = (float)v * (1.0f / 146.0f);       /* the C-stick reports about +-0x9C */
    float a = f < 0.0f ? -f : f;
    if (a <= 0.15f) return 0.0f;
    a = (a - 0.15f) * (1.0f / 0.85f);
    if (a > 1.0f) a = 1.0f;
    a *= a;
    return f < 0.0f ? -a : a;
}

/* 0x2645CC: L/R handling in the yaw controller (logic tick).  Same gate as
 * the game's own L/R check: input not locked, no snap cool-down. */
void cam_stick_yaw(u32 cam)
{
    if (*(volatile u8 *)(MAPFLAGS + 0x18) || *(volatile u32 *)(MAPFLAGS + 0x1C)) return;
    if (*(volatile u8 *)(cam + 0xB0)) return;            /* L+R snap in progress */
    if (*(volatile s16 *)(cam + 0xB2) > 6) return;
    if (C->state == 0) {
        stick_init();
        /* debug: one visible 45 degree jump = hook reached and ir:rst opened */
        if (C->state == 1 && (cam_cfg.flags & 8)) *(volatile float *)(cam + 0x18) += 45.0f;
    }
    if (C->state != 1) {
        if (cam_cfg.flags & 8) *(volatile float *)(cam + 0x18) += 1.0f;   /* visible "hook ran, ir:rst failed" */
        return;
    }
    volatile u32 *m = (volatile u32 *)C->blk[2];
    u32 v = m[6 + (m[4] & 7) * 4 + 3];           /* newest entry: s16 x | s16 y << 16 */
    float x = axis((s16)v), y = axis((s16)(v >> 16));
    u32 fl = cam_cfg.flags;
    if (fl & 1) x = -x;
    if (fl & 2) y = -y;
    *(volatile float *)(cam + 0x18) -= x * cam_cfg.yawSpeed;   /* L (+yaw) turns left */
    float p = C->pitch - y * cam_cfg.pitchSpeed;               /* stick up = look up = camera lower */
    if (p < cam_cfg.pitchMin) p = cam_cfg.pitchMin;
    if (p > cam_cfg.pitchMax) p = cam_cfg.pitchMax;
    C->pitch = p;
}

/* 0x266284: camera placement, right after the camera height is stored. */
/* current tilt; while an L+R snap runs it eases back to level */
static float cur_pitch(void)
{
    float p = C->pitch;
    u32 cam = *(volatile u32 *)CAM_PTR;
    if ((cam_cfg.flags & 4) && cam >= 0x00100000u && *(volatile u8 *)(cam + 0xB0)) {
        p *= 0.7f;
        if (p < 0.002f && p > -0.002f) p = 0.0f;
        C->pitch = p;
    }
    return p;
}

/* rotate pos[0..2] up/down around (tx,ty,tz) by p radians, keeping the distance */
static void tilt(volatile float *pos, float tx, float ty, float tz, float p)
{
    float vx = pos[0] - tx, vy = pos[1] - ty, vz = pos[2] - tz;
    float hl = __builtin_sqrtf(vx * vx + vz * vz);
    if (!(hl > 1.0f)) return;
    float p2 = p * p;                            /* |p| <= ~0.7: Taylor is exact enough */
    float s = p * (1.0f - p2 * (1.0f / 6.0f) * (1.0f - p2 * (1.0f / 20.0f)));
    float c = 1.0f - p2 * 0.5f * (1.0f - p2 * (1.0f / 12.0f) * (1.0f - p2 * (1.0f / 30.0f)));
    float nh = hl * c - vy * s;
    float nv = hl * s + vy * c;
    if (nh < hl * 0.2f) nh = hl * 0.2f;          /* never swing over the top */
    float k = nh / hl;
    pos[0] = tx + vx * k;
    pos[1] = ty + nv;
    pos[2] = tz + vz * k;
}

/* 0x266284: camera placement of the MAP-state camera (FUN_0026617C), before
 * its collision/smoothing: camera map+0x60, look-at map+0x6C */
void cam_stick_pitch(u32 map)
{
    float p = cur_pitch();
    if (p == 0.0f) return;
    volatile float *f = (volatile float *)map;
    tilt(f + 24, f[27], f[28], f[29], p);
}

/* 0x25C484: FUN_0025C470(pos s0-s2, target s3-s5) hands the free-roam
 * camera (mode 0, FUN_00277xxx) to the game camera.  Only the stack copy of
 * the position is tilted; the controller's own state (cam+0x4C) is untouched,
 * so the game's smoothing never sees it. */
void cam_stick_view(volatile float *pos, const float *in)
{
    u32 cam = *(volatile u32 *)CAM_PTR;
    if (cam < 0x00100000u || *(volatile u32 *)cam != 0) return;   /* free-roam mode only */
    float p = cur_pitch();
    if (p == 0.0f) return;
    tilt(pos, in[3], in[4], in[5], p);
}

/* ---- hook entries --------------------------------------------------- */
__asm__(
"    .section .cam.text.hooks,\"ax\",%progbits\n"
"    .arm\n"
"    .global cam_yaw_hook, cam_yaw2_hook, cam_pitch_hook, cam_view_hook\n"
/* bl from 0x25C484, replaces "vstmia r1, {s0, s1, s2}" (r1 = stack copy of pos) */
"cam_view_hook:\n"
"    vstmia r1, {s0, s1, s2}\n"
"    push  {r0, r1, r2, r3, r12, lr}\n"
"    vpush {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    mov   r0, r1\n"
"    mov   r1, sp\n"
"    bl    cam_stick_view\n"
"    vpop  {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    pop   {r0, r1, r2, r3, r12, lr}\n"
"    bx    lr\n"
/* bl from 0x2D22FC (mode-0 camera, FUN_002D1DA0), replaces "ldr r0, [r9, #4]" (r9 = 0x4474F0) */
"cam_yaw2_hook:\n"
"    push  {r0, r1, r2, r3, r12, lr}\n"
"    vpush {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    ldr   r0, [r9, #4]\n"
"    bl    cam_stick_yaw\n"
"    vpop  {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    pop   {r0, r1, r2, r3, r12, lr}\n"
"    ldr   r0, [r9, #4]\n"
"    bx    lr\n"
/* bl from 0x2645CC, replaces "cmp r5, #0" (r8 = &camera pointer) */
"cam_yaw_hook:\n"
"    push  {r0, r1, r2, r3, r12, lr}\n"
"    vpush {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    ldr   r0, [r8]\n"
"    bl    cam_stick_yaw\n"
"    vpop  {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    pop   {r0, r1, r2, r3, r12, lr}\n"
"    cmp   r5, #0\n"
"    bx    lr\n"
/* bl from 0x266284, replaces "vstr s0, [r4, #100]" (r4 = map object) */
"cam_pitch_hook:\n"
"    vstr  s0, [r4, #100]\n"
"    push  {r0, r1, r2, r3, r12, lr}\n"
"    vpush {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    mov   r0, r4\n"
"    bl    cam_stick_pitch\n"
"    vpop  {d0, d1, d2, d3, d4, d5, d6, d7}\n"
"    pop   {r0, r1, r2, r3, r12, lr}\n"
"    bx    lr\n"
);
