# Feasibility: Counter-Strike with local splitscreen and bots

A custom N64 implementation is practical. A direct recompile of this PC game
DLL is not a useful starting point: `dlls/h_export.cpp` depends on engine callbacks
from `engine/eiface.h`, and the repository does not supply an N64 renderer,
platform layer, asset system, or local multiplayer frontend. Selectively porting
movement, combat math, weapon timing, and round rules can follow a working N64
runtime.

The relevant precedent is the existing `halo-1/port/n64` project. Its README and
runtime describe a standalone libdragon/Tiny3D demake, shared resident assets,
per-player cameras, reduced textures/models, offline conversion, and measured
emulator checks. It is not a general Xbox-to-N64 compiler. Its own four-player
performance notes also show why splitscreen needs measurement rather than a
promise of 60 FPS. This proof of concept reuses the installed SDKs and the overall
approach, not Halo's runtime or game assets.

## What the initial walkthrough established

The original local `de_dust2.bsp` converts without reducing its selected brush
polygons. Sky, clip, origin, and trigger surfaces are excluded from drawing;
world collision still uses the BSP standing-player hull. Solid static brush
models are tested separately. Visibility comes from original world leaves and
PVS rows, followed by face and conservative view culling. Material-sorted drawing
avoids repeated scans and redundant texture uploads.

| Measured content | Size/count |
| --- | ---: |
| Rendered map triangles, before visibility culling | 9,932 |
| Packed vertex data | 308,704 bytes |
| RGBA16 textures, 34 × 32×32 | 69,632 bytes |
| Original BSP planes / clipnodes | 9,582 / 8,321 |
| Source spawns | 40 |
| World visibility leaves | 1,201 |
| Linked text + data + BSS | Approximately 1.10 MiB |
| Three color buffers + one depth buffer at 320×240×16-bit | 600 KiB |

Those linked sizes include the static map data but exclude dynamic SDK/renderer
allocations. They are not a measured free-heap figure. The ROM boots and draws
on ares with 4 MiB RAM. Expansion Pak support is a sensible target for the full
game, especially with character meshes, animation, weapons, audio, and bots.

The optimized normal build's counter shows approximately 60 frames per emulated
second at the checked T-spawn view. This is a rendered-loop counter, not a VI
presentation audit or a map-wide benchmark. Instrumented graphics validation is
slower. Neither measurement establishes real-console performance or input latency.

## Current two-player milestone

Two-player horizontal splitscreen now runs on the original Dust II map. Both
players have the original Terrorist model, a hand-attached Glock, first-person
hands/weapon, skeletal animations, crouching, hitscan damage, wall occlusion,
reloads, death and respawn. Weapon audio assets/mixing are present, but playback
startup needs the fix identified in `PERFORMANCE.md`. Assets and simulation are shared;
controllers, cameras, viewport matrices, HUD and visibility selection are separate.

The original player model has 740 triangles and 53 bones; the first-person Glock
and hands have 882 triangles and 41 bones; the held Glock has 78 triangles.
All 111 player clips and 13 viewmodel clips remain in the ROM's compressed source
animation streams. Runtime caching avoids repeated RLE decoding and Euler
conversion. Indexed 64-vertex batches and cached RSP command blocks reduce draw
submission and vertex transformation costs. Deformed vertices and matrices have
three fenced buffers so the CPU cannot overwrite data still consumed by the RSP.

This version targets an 8 MiB Expansion Pak. Linked code, constant data and BSS
are approximately 3.33 MiB, before framebuffers, mesh buffers, command blocks,
audio buffers and SDK allocations. The build manifest and runtime heap logs give
the exact measurements for each build.

The uninstrumented two-player scripted T-spawn route currently measures roughly
17–19 rendered frames per emulated second. It is a working prototype, below the
30 FPS target. The RDP validator is slower and is not a performance benchmark.
The automated acceptance fixture demonstrates movement, jump/crouch, shooting,
wall occlusion (host test), reload, kill/death and respawn. Source fidelity checks
compare 103,239 bone poses, and host sanitizer tests exercise every animation
frame. Physical-controller and real-console validation remain outstanding.

## More views and performance

Four views do not require four copies of the map, but CPU traversal, draw
submission, clipping, character animation, and visible geometry can multiply.
The smaller pixel area per view does not remove those costs.

Use 320×240 overall as the baseline. Aim for a measured 30 FPS two-player build
before committing to four-player content budgets. Add an RDP completion/VI
observer and benchmark worst-case opposing views at mid, long A, B, and tunnels.
The current per-face submission is deliberately simple; compact cached geometry
batches, per-leaf candidate lists, and more efficient lighting will likely matter
once more views and animated actors are present.

## Bots

Bots are feasible with bounded navigation and perception work. Start with one
actor that follows a compact waypoint graph through the existing collision
controller, then add line of sight, aiming, firing, team goals, and round state.
Build navigation offline and stagger pathfinding/perception updates; keep movement
on the fixed simulation tick. Increase bot counts only after measuring memory
and CPU costs alongside splitscreen.

A local `cs16-lan/assets/steam/czero/maps/de_dust2.nav` exists. Its map compatibility
and format have not been validated for this CS 1.6 BSP, so it is a candidate input,
not an implemented navigation system. Full Condition Zero bot behavior is a
separate porting effort. Initial bot count and four-player performance remain open.

## Suggested sequence and acceptance gates

1. **Walkthrough (implemented):** real map data, one first-person controller, wall/floor
   collision, gravity, jumping, emulator boot, and scripted movement checks.
   Next fidelity checks include every route, stairs, ceiling/edge cases, and
   physical gamepad input.
2. **Combat slice (implemented):** original Terrorist and Glock meshes, hitscan, damage,
   animation, sound assets/mixing, death, respawn, and a repeatable memory/performance baseline.
3. **Two-player combat (implemented):** independent views/input and shared simulation.
   Next: stable 30 FPS across representative routes, physical gamepads, team rules
   and a real round reset. Current players are both Terrorists with friendly fire.
4. **One bot:** navigation route following plus perception and combat, then a
   small measured bot roster.
5. **Counter-Strike rules and four views:** buys, inventory, bomb objective,
   grenades, team wins, audio mix, frontend, and deliberate content/performance
   budgets. Validate on an actual N64 before describing it as hardware-ready.

No calendar estimate is justified by a single rendering prototype. The basic
walkthrough has low technical uncertainty now; faithful weapon/round rules, faster
multiplayer rendering, and performant AI are substantial remaining work.

## Current omissions and boundaries

The current runtime has player models, skeletal animations, a basic Glock, two
views and a weapon audio pipeline whose playback startup is currently inactive.
Ladders, water movement, dynamic doors, breakable objects,
dynamic lights, skybox imagery, networking, bots and Counter-Strike match logic
remain unimplemented. Player damage uses simple boxes. Vertex lighting, reduced
textures, movement and animation transitions are visible approximations. Original
standing, crouching and point BSP hulls supply collision and bullet occlusion.

Only source and build tooling belong in this checkout. Extracted game data and
ROMs remain local. Existing Valve source files retain their original notices;
the proof does not incorporate those game-DLL implementations.

Primary technical references:

- [libdragon](https://github.com/DragonMinded/libdragon)
- [Tiny3D](https://github.com/HailToDodongo/tiny3d)
- [GoldSrc BSP structures in Xash3D](https://github.com/FWGS/xash3d-fwgs/blob/master/common/bspfile.h)
- [ares v148 Quartz keyboard mapping](https://github.com/ares-emulator/ares/blob/v148/ruby/input/keyboard/quartz.cpp)

Local source references: `../halo-1/port/n64/README.md`, `scene.c`, `build.py`,
and the SDK headers; `../cs16-lan/src/xash3d-fwgs/common/bspfile.h` for the binary
layout; this repository's `dlls/h_export.cpp` and `engine/eiface.h` for engine
coupling. Paths here are relative to the repository root where appropriate.
