/* ===== (shared header) =====================================================================
 * SMT IV 60 FPS - Phase 2b: render interpolation for camera and objects
 *
 * Game logic still runs at 30 Hz.  On a logic frame the scene is
 * rendered at the midpoint between the previous and the current logic
 * state, on the in-between frame at the current state:
 *      ..., (S0+S1)/2, S1, (S1+S2)/2, S2, ...
 * which is evenly spaced 60 Hz motion (about 8 ms extra latency).
 * The game itself never sees the midpoint values: they are written into
 * the NW4C nodes only around AsdModelCalcProc's scene update and then
 * restored.
 * ===================================================================== */
typedef unsigned int  u32;
typedef unsigned char u8;
typedef unsigned short u16;

#define NREC 15
typedef struct { u32 node; u32 tick; float p[3]; } Rec;           /* 20 bytes */
typedef struct {
    u32   counter;      /* +0x00 frame counter                         */
    u8    logic;        /* +0x04 1 on logic frames                     */
    u8    camValid;     /* +0x05                                       */
    u8    pad[2];
    float dt;           /* +0x08 per-task time step (1.0 / 0.5)        */
    u32   camTick;      /* +0x0C                                       */
    float camPrev[9];   /* +0x10 pos, target, up of previous tick      */
    float camCur[9];    /* +0x34                                       */
    float camTrue[9];   /* +0x58 true pos/target/up while swapped      */
    u32   camPtr;       /* +0x7C camera object currently holding midpoint */
    u8    camSwapped;   /* +0x80                                       */
    u8    heapDead;     /* +0x81 allocation failed / corrupted         */
    u8    emu;          /* +0x82 1 = running on Azahar/Citra (extension enabled) */
    u8    emuChecked;   /* +0x83                                       */
    u32   heap;         /* +0x84 sprite table (game heap)              */
    u32   pad3[2];      /* keep rec[] at +0x90                         */
    Rec   rec[NREC];    /* +0x90 nodes moved by logic this tick        */
} State;                /* 0x90 + 15*20 = 0x1BC bytes                  */

#define S ((State *)0x004EFE38)          /* ends 0x4EFFF4 < 0x4F0000 */
_Static_assert(sizeof(State) <= 0x4F0000 - 0x4EFE38, "state too big");

#ifdef REGION_APOC
#define CAM_WIDE (*(volatile u32 *)((u32)S + 0x6A4))   /* the camera move continues last tick's: blend past THR2 */
#else
#define CAM_WIDE (S->pad[0])                            /* in battle (cam_cut): blend past THR2 */
#endif

/* NW4C TransformNode: local translation, transform flags */
#define NODE_TX(n)    (*(volatile float *)((n) + 0x58))
#define NODE_TY(n)    (*(volatile float *)((n) + 0x68))
#define NODE_TZ(n)    (*(volatile float *)((n) + 0x78))
#define NODE_FLAGS(n) (*(volatile u32 *)((n) + 0x88))
#define FLAG_LOCAL_DIRTY 0x800u

/* jumps larger than this per logic tick are cuts/teleports: no blend */
#define THR2 (150.0f * 150.0f)


extern void *GetGameCamera(void);   /* FUN_002C5218 */
extern void  CamSetLookAt(void *cam, const float *pos, const float *tgt, const float *up); /* FUN_002C20A8 */
extern void  CamUpdateStereo(void *cam);  /* FUN_002BCBB0 */
extern void  ModelCalc(void);       /* FUN_00115B74 */
extern u32   GameAlloc(u32 a, u32 b, u32 c, u32 size, u32 e);   /* FUN_002C4B84 */
static inline u32 tick_now(void) { return S->counter >> 1; }

#define SPR_N     256
#define SPR_MAGIC 0x50463453u
#define SPR_POS_THR (256 << 12)   /* fast slides (win screen, info panel) move >64 px per tick */
typedef struct {
    u32 spr, tick, swapped;
    int px, py; u32 pa, pb;                       /* before this tick's update (pa/pb = float bits) */
    int cx, cy; u32 ca, cb;                       /* true values while swapped */
    int qx, qy; u32 qa, qb;                       /* values the animator left (exit) */
} SRec;
/* Watch list: plain values the game updates per logic tick and the draw
 * reads directly (e.g. the player position the bottom-screen map is
 * centred on).  Each def: address = (ptr ? *ptr : 0) + off, n floats. */
typedef struct { u32 ptr, off, n, phase; } WatchDef;  /* n (<=3) floats at (ptr ? *ptr : 0) + off; phase 0 = draw start, 1 = camp draw */
#define NWATCH 5
typedef struct { u32 addr, tick, swapped; u32 last[3]; u32 cur[3]; } Watch;
typedef struct { u32 magic; u32 pad[3]; SRec e[SPR_N]; Watch w[NWATCH]; } SprTab;
#define SPX(p) (*(volatile int *)((p) + 0x0C))
#define SPY(p) (*(volatile int *)((p) + 0x10))
#define SPA(p) (*(volatile u32 *)((p) + 0x20))
#define SPB(p) (*(volatile u32 *)((p) + 0x24))
SprTab *spr_tab(void);
void cam_apply_bits(u32 cam, const u32 *v);

/* Extension (Azahar-only, lives in the rodata page tail; only called when S->emu) */
void ext_watch_swap(SprTab *T, u32 t, u32 phase);
void ext_watch_restore(SprTab *T);
