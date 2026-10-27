@ =====================================================================
@  SMT IV (USA, 00040000000E5C00) - 60 FPS "logic gate" framework
@  Phase 1: 60 Hz presentation, game logic held at its original 30 Hz.
@
@  Every displayed frame toggles a parity bit.  On "logic" frames the
@  game runs exactly as it did at 30 fps.  On the in-between frames only
@  draw tasks (and any whitelisted 60 Hz-safe tasks) run.
@
@  Assembled for ARMv6K (ARM11 MPCore), linked at the code cave that
@  sits in the zero padding at the end of .text (0x3FE640 - 0x3FF000).
@  State lives in the unused tail of the .bss page (0x4EFF00).
@ =====================================================================
        .syntax unified
        .arm
        .section .text.cave,"ax",%progbits

        .equ STATE,        0x004EFE38   @ shared with interp.c (struct State)
        .equ DISP,         0x004E39E0   @ display manager (vsync interval @+0x1C0)
        .equ FRAMETIME60,  0x4682937D   @ 16713.74f us = 1e6 / 59.831

        @ game functions (absolute, passed in via --defsym)
        .global hk_frame_begin, hk_fade, hk_playtime_flag, hk_sound_flag
        .global hk_task_gate, inc_r0, inc_r2, inc_r0_from_r1, hk_camp
        .global hk_updframe, hk_blend, hk_setpos_a, hk_setpos_b, hk_anim_entry, hk_anim_slide, hk_anim_v2, hk_anim_v3, hk_beforedraw, hk_campdraw, hk_runend

@ ---------------------------------------------------------------------
@ 0x101B8C  "bl InputUpdate" (FUN_0010B578) at the top of the main loop.
@ Advances the frame parity and runs input only on logic frames, so that
@ the per-button hold counters (FUN_001078D8, "pressed" == count 1) keep
@ their 30 Hz meaning and no press is ever lost on an in-between frame.
@ Also pins the vsync interval to 0 (one vblank per frame) and the
@ frame-time float the CPU-load check divides by.
@ ---------------------------------------------------------------------
hk_frame_begin:
        ldr     r0, =STATE
        ldr     r1, [r0]
        add     r1, r1, #1
        str     r1, [r0]
        and     r2, r1, #1
        eor     r2, r2, #1              @ logic frame when counter is even
        strb    r2, [r0, #4]
        mov     r1, #0x3F800000         @ g_dt = 1.0 outside tasks
        str     r1, [r0, #8]
        ldr     r0, =DISP
        mov     r1, #0
        str     r1, [r0, #0x1C0]        @ vsync interval = 0  -> 60 Hz
        ldr     r1, =FRAMETIME60
        str     r1, [r0, #0x1C4]        @ frame time used by load check
        push    {r4, lr}
        bl      frame_c                 @ heap init + put the true camera back before logic
        pop     {r4, lr}
        ldr     r0, =STATE
        ldrb    r0, [r0, #4]
        cmp     r0, #0
        bxeq    lr
        b       InputUpdate             @ tail call FUN_0010B578

@ ---------------------------------------------------------------------
@ 0x101E0C  "bl ScreenFadeUpdate" (FUN_00102438): frame-counted global
@ top/bottom screen fades.  Logic frames only.
@ ---------------------------------------------------------------------
hk_fade:
        ldr     r0, =STATE
        ldrb    r0, [r0, #4]
        cmp     r0, #0
        bxeq    lr
        b       ScreenFadeUpdate        @ tail call FUN_00102438

@ ---------------------------------------------------------------------
@ 0x101E10  "ldrb r0,[r5,#4]" gating the play-time block:
@   FUN_00105914 (frame-counted SE/FX delays),
@   play-time seconds (30-frame counter),
@   FUN_00105338 (in-game clock, 1800 frames = 1 minute).
@ Returning 0 on in-between frames skips the whole block.
@ ---------------------------------------------------------------------
hk_playtime_flag:
        ldr     r0, =STATE
        ldrb    r0, [r0, #4]
        cmp     r0, #0
        ldrbne  r0, [r5, #4]
        bx      lr

@ ---------------------------------------------------------------------
@ 0x101E98  "ldrb r0,[r4]" gating the sound/stream block
@   FUN_001062E0 / FUN_00105F3C / FUN_001066AC (has 16-frame delays).
@ Non-zero skips the block, so return 1 on in-between frames.
@ ---------------------------------------------------------------------
hk_sound_flag:
        ldr     r0, =STATE
        ldrb    r0, [r0, #4]
        cmp     r0, #0
        moveq   r0, #1
        ldrbne  r0, [r4]
        bx      lr

@ ---------------------------------------------------------------------
@ 0x103070  "ldrsh r0,[r4,#0x34]" in the task scheduler run loop
@ (FUN_00102F18).  r4 = task.  The original code does
@     cmp r0,#0 ; bgt next_task
@ so returning 1 skips the task for this frame, 0 lets it run.
@ Skipped on in-between frames:
@   - all logic tasks (priority < 0x65): MAP C, BTLMAIN C, MESS_C, ...
@   - priority 0x67: AsdModelCalcProc / AsdMdlManCalcProc (model calc)
@ unless the task is one of the two 60 Hz-safe model tasks (name hash check).
@ Draw tasks (>= 0x65, != 0x67) always run.
@ ---------------------------------------------------------------------
@ Phase 2: the gate also publishes the per-task time step g_dt at STATE+8:
@   1.0 for tasks that run at 30 Hz (gated logic tasks)
@   0.5 for tasks that run every frame (draw tasks, whitelisted tasks)
@ Any per-frame step patched to multiply by g_dt is therefore correct no
@ matter which task executes it.
hk_task_gate:
        ldrsh   r0, [r4, #0x34]         @ original: start-delay counter
        cmp     r0, #0
        bxgt    lr
        push    {r1-r3}
        ldr     r1, [r4, #0x20]         @ priority
        cmp     r1, #0x65
        blt     .Lwl
        cmp     r1, #0x67
        bne     .Levery                 @ draw task: every frame
.Lwl:
        ldr     r2, [r4, #0x18]         @ name hash (sum of name chars)
        sub     r2, r2, #0x610          @ "AsdModelCalcProc"  (0x610)
        cmp     r2, #0
        cmpne   r2, #0x48               @ "AsdMdlManCalcProc" (0x658)
#ifdef REGION_APOC
        cmnne   r2, #0x19               @ "h3dModelCalcProc"  (0x5F7): Apocalypse's model system (hk_h3d_model)
#endif
        bne     .Lgated
.Levery:                                @ runs every frame, dt = 0.5
        ldr     r1, =STATE
        mov     r2, #0x3F000000
        str     r2, [r1, #8]
        mov     r0, #0
        pop     {r1-r3}
        bx      lr
.Lgated:                                @ 30 Hz task, dt = 1.0
        ldr     r1, =STATE
        mov     r2, #0x3F800000
        str     r2, [r1, #8]
        ldrb    r1, [r1, #4]
        cmp     r1, #0
        moveq   r0, #1                  @ in-between frame: skip
        movne   r0, #0                  @ logic frame: run
        pop     {r1-r3}
        bx      lr

@ ---------------------------------------------------------------------
@ "add rX, rY, #1" replacements for frame counters that live in draw
@ tasks.  They add the logic flag (1 or 0) instead of 1.
@ They preserve ALL other registers AND the condition flags.
@ ---------------------------------------------------------------------
inc_r0:
        push    {r12}
        ldr     r12, =STATE
        ldrb    r12, [r12, #4]
        add     r0, r0, r12
        pop     {r12}
        bx      lr

inc_r2:
        push    {r12}
        ldr     r12, =STATE
        ldrb    r12, [r12, #4]
        add     r2, r2, r12
        pop     {r12}
        bx      lr

inc_r0_from_r1:
        mov     r0, r1                  @ flags untouched
        b       inc_r0

@ ---------------------------------------------------------------------
@ 0x108878  "add r1, pc, #504" in CAMPDL (FUN_00108840, camp-menu
@ background).  0x108878..0x108A60 is animation update (glow ramp,
@ 24 staggered sprite animations), 0x108A64 is the actual draw.
@ On in-between frames jump straight to the draw.
@ ---------------------------------------------------------------------
hk_camp:
        push    {r0}
        ldr     r0, =STATE
        ldrb    r0, [r0, #4]
        cmp     r0, #0
        pop     {r0}
        ldrne   r1, =0x00108A78         @ value the original add produced
        bxne    lr
        ldr     lr, =0x00108A64         @ skip update, draw only
        bx      lr

@ ---------------------------------------------------------------------
@ 0x36880C  NW4C AnimObject::UpdateFrame (vtable slot +0x14 of the three
@ anim-object classes, vtables 0x420D6C / 0x4210B0 / 0x4211B8):
@     step = this->+0x40; if (step == 0) return;
@     this->frame(+0x38) = policy(start(+0x44), end(+0x48), frame + step)
@ Entered with "b" (no link) so lr still holds the caller's return.
@ Replaces "vldr s0,[r0,#0x40]" with  s0 = step * g_dt.
@ ---------------------------------------------------------------------
hk_updframe:
        vldr    s0, [r0, #0x40]
        ldr     r12, =STATE
        vldr    s1, [r12, #8]
        vmul.f32 s0, s0, s1
        b       UpdateFrame_cont        @ 0x368810 (s1 is reloaded there)

@ ---------------------------------------------------------------------
@ 0x12477C  model-manager animation crossfade (FUN_00124714):
@     blendElapsed(+0x40) = blendElapsed + 1.0 (s19)
@ -> blendElapsed + 1.0 * g_dt.  Preserves every register except s0.
@ ---------------------------------------------------------------------
hk_blend:
        push    {r12}
        vpush   {d1}
        ldr     r12, =STATE
        vldr    s2, [r12, #8]
        vmul.f32 s2, s2, s19
        vadd.f32 s0, s0, s2
        vpop    {d1}
        pop     {r12}
        bx      lr
#ifdef REGION_APOC
@ ---------------------------------------------------------------------
@ Apocalypse h3d models (characters): "h3dModelCalcProc" (0x145F94) loops
@ over its models and calls the per-model update 0x156994(model, r1, step s0),
@ which builds the root matrix from the model's position (+0x1B4), steps
@ its animations by s0 and updates the skeleton.  The task gate now runs
@ it every frame, so the step is scaled by g_dt, and h3d_pre/h3d_post draw
@ the model at the midpoint of the last two ticks on logic frames.
@ bl from 0x146020 (the loop's call).
@ ---------------------------------------------------------------------
        .global hk_h3d_model
hk_h3d_model:
        push    {r4-r8, lr}
        vpush   {d8}
        mov     r4, r0
        mov     r5, r1
        ldr     r12, =STATE
        vldr    s16, [r12, #8]
        vmul.f32 s16, s16, s0           @ anim step * g_dt
        bl      h3d_pre                 @ r0 = model
        mov     r0, r4
        mov     r1, r5
        vmov.f32 s0, s16
        bl      H3dModelUpdate          @ 0x156994
        mov     r0, r4
        bl      h3d_post
        vpop    {d8}
        pop     {r4-r8, pc}

@ SMT IV: Apocalypse adds a variable step ("vadd s0, s0, s2"): scale s2 instead of s19.
        .global hk_blend_s2
hk_blend_s2:
        push    {r12}
        vpush   {d2}
        ldr     r12, =STATE
        vldr    s4, [r12, #8]
        vmul.f32 s4, s4, s2
        vadd.f32 s0, s0, s4
        vpop    {d2}
        pop     {r12}
        bx      lr
#endif

@ ---------------------------------------------------------------------
@ Atlus model position setters (leaf functions, entered with b):
@   0x28C8A8 SetPos(model, _, const vec3*):  ldr r0,[r0,#0x2C] ; ldr r3,[r2] ; ...
@   0x29A2B0 SetPos(model, s0, s1, s2):      ldr r0,[r0,#0x2C] ; vstr s0,[r0,#0x58] ; ...
@ The 2nd instruction is replaced by "b stub"; r0 already holds the
@ NW4C node.  rec_node() remembers the pre-write translation.
@ ---------------------------------------------------------------------
hk_setpos_a:
        push    {r0-r3, r12, lr}
        bl      rec_node
        pop     {r0-r3, r12, lr}
        ldr     r3, [r2]                @ replaced instruction
        b       SetPosA_cont            @ 0x28C8B0

hk_setpos_b:
        push    {r0-r3, r12, lr}
        vpush   {d0-d1}                 @ s0-s2 are the arguments
        bl      rec_node
        vpop    {d0-d1}
        pop     {r0-r3, r12, lr}
        vstr    s0, [r0, #0x58]         @ replaced instruction
        b       SetPosB_cont            @ 0x29A2B8

        .ltorg

@ ---------------------------------------------------------------------
@ Phase 3b: 2D sprite state swapping (see interp.c)
@ ---------------------------------------------------------------------
@ Sprite animator wrappers.  Each variant entry jumps to anim_common with
@ the address of its own "displaced push + branch back" stub in r12.
@ anim_common records the pre-update state, calls the original animator as
@ a subroutine, records the post-update state and returns its result.
@ All variants take (sprite, int args in r1/r2) and return an int in r0.
        .macro ANIM_HOOK name, regs, cont
\name:
        adr     r12, 1f
        b       anim_common
1:      push    \regs                   @ displaced instruction
        b       \cont
        .endm
        ANIM_HOOK hk_anim_entry, "{r4-r8, lr}",      Anim_cont      @ 0x2BF9F0 main animator (296 callers)
        ANIM_HOOK hk_anim_slide, "{r4, r5, r6, lr}", AnimSlide_cont @ 0x2B9220 slide-in variant (100 callers)
        ANIM_HOOK hk_anim_v2,    "{r4-r8, lr}",      AnimV2_cont    @ 0x29538C vertical slide variant (8)
        ANIM_HOOK hk_anim_v3,    "{r4, r5, r6, lr}", AnimV3_cont    @ 0x296528 variant (1)

anim_common:
        push    {r0-r3, r12, lr}
        vpush   {d0-d7}
        bl      spr_pre                 @ r0 = sprite
        vpop    {d0-d7}
        pop     {r0-r3, r12, lr}
        push    {r0, lr}                @ keep sprite + caller's return
        blx     r12                     @ original animator
        pop     {r1, lr}
        push    {r0, lr}                @ keep result
        mov     r0, r1
        bl      spr_post
        pop     {r0, pc}

@ 0x103B58 BeforeDraw1L (first draw task, prio 0x65): "mov r0,#0" displaced.
hk_beforedraw:
        mov     r0, #0                  @ phase 0: draw start (also the displaced "mov r0,#0")
        push    {r0, lr}
        bl      spr_swap
        pop     {r0, lr}
        b       BeforeDraw_cont         @ 0x103B5C

@ 0x108A64 CAMPDL "mov r0,#1" before its draw: camp sprites are animated
@ inside this draw task, after the BeforeDraw1L swap, so swap them here.
hk_campdraw:
        mov     r0, #1                  @ phase 1: camp draw (also the displaced "mov r0,#1")
        push    {r0, lr}
        bl      spr_swap
        pop     {r0, lr}
        bx      lr

@ 0x1031F4 end of the scheduler run pass "ldr r0,[sp]": restore true
@ sprite values before the kill pass can free anything.
hk_runend:
        push    {r4, lr}
        bl      spr_restore
        pop     {r4, lr}
        ldr     r0, [sp]                @ displaced instruction (caller's sp)
        bx      lr

@ ---------------------------------------------------------------------
@ ARM veneers for game functions called from Thumb C code: they make the
@ linker emit BLX (mode switch) instead of a Thumb BL into ARM code.
@ ---------------------------------------------------------------------
        .global CamSetLookAt, CamUpdateStereo, GameAlloc
        .type   CamSetLookAt, %function
        .type   CamUpdateStereo, %function
        .type   GameAlloc, %function
CamSetLookAt:    b  CamSetLookAt_abs      @ FUN_002C20A8
CamUpdateStereo: b  CamUpdateStereo_abs   @ FUN_002BCBB0
GameAlloc:       b  GameAlloc_abs         @ FUN_002C4B84
