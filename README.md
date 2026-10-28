# Counter-Strike on Nintendo 64: two-player Dust II prototype

`port/n64/` builds a standalone libdragon/Tiny3D ROM with two-player horizontal
splitscreen on the original Counter-Strike 1.6 Dust II map. Both players use the
original **Terrorist / Phoenix** model and a **Glock 18**. This is a custom N64
runtime; it does not run GoldSrc or compile the PC game DLL.

Implemented:

- Two independent controllers, cameras, HUDs, movement and weapon states.
- Original textured map, BSP visibility, standing and crouching collision hulls,
  sliding, stairs, jumping and player blocking.
- Original skeletal player and weapon meshes. Idle, walking, running, backwards
  movement, crouching, jumping, aiming, firing, reloading and death are animated.
  Upper-body actions blend over leg movement, with nine-way yaw/pitch aim blending
  and transitions between actions. The third-person Glock follows the hand bones.
- First-person Glock draw, idle, firing and reload animations.
- Semi-automatic hitscan fire, 20-round magazines, 120 reserve rounds, 25 damage,
  wall occlusion, hit feedback, kills and automatic respawning after four seconds.
  **Friendly fire is enabled** so the two Terrorists can test combat together.

Original weapon, magazine and slide sounds play through the stereo mixer.
Audio DMA is primed at startup and active callbacks are included in profiling.

The complete source clip libraries are retained: 111 Terrorist sequences, 13
Glock viewmodel sequences and the held-weapon sequence. Runtime state selection
uses the movement and Glock actions above. Other weapon actions and swimming
clips are available in the data, but their gameplay is not implemented.

## Play in ares

The ROM requires **8 MiB RAM / Expansion Pak**. From this directory:

```sh
python3 port/n64/run_ares.py
```

The launcher opens `build/n64/cs64-dust2-2p.z64` with a separate project profile,
`build/n64/ares-two-player.bml`. It enables the Expansion Pak and maps two
controllers. It uses the isolated `build/n64/CS64-ares.app` when present, otherwise
`/Applications/ares.app`. Global ares settings are not edited. The keyboard
mapping targets ares v148's macOS Quartz driver. Focus the game window to play.

| Action | Player 1 keyboard | Player 2 keyboard | N64 controller |
| --- | --- | --- | --- |
| Forward / back | W / S | I / K | Stick up / down |
| Strafe left / right | A / D | J / L | C-left / C-right |
| Turn left / right | Left / Right arrows | U / O | Stick left / right |
| Look up / down | Up / Down arrows | 8 / 9 | C-up / C-down |
| Jump | Space | N | A |
| Fire (press per shot) | Q | M | Z |
| Reload | E | P | B |
| Crouch (hold) | C | V | L |
| Respawn | Return | Delete / Backspace | Start |

The N64 D-pad also moves and strafes. With physical gamepads, bind Gamepad input
for Nintendo 64 controller ports 1 and 2 in this project profile. Existing custom
bindings are preserved on subsequent launches; `--reset-profile` recreates only
the project keyboard bindings. Two gamepads avoid keyboard rollover limits.

The two players begin at nearby original T spawns facing one another. Occupied
respawn points are skipped. There is no round or bomb objective yet.

## Build

Requires Python 3 (standard library only), a host C compiler, a built libdragon
toolchain and Tiny3D. The default 16 KiB command queues also require the matching
libdragon source checkout beside the SDK as `libdragon-src`, or supplied with
`--libdragon-source`. `--queue-kib 2` uses the installed SDK without an override.
The private `cops-and-robbers/` submodule supplies the build assets for authorized
collaborators:

```sh
git submodule update --init cops-and-robbers
```

A public clone can instead build from a local game installation using `--assets`.
With the submodule initialized:

```sh
python3 port/n64/build.py
```

Default SDK paths reuse the installations used by `../halo-1/port/n64/`.
Alternative locations can be supplied explicitly:

```sh
python3 port/n64/build.py \
  --assets /path/to/steam/game-data \
  --sdk /path/to/libdragon \
  --tiny3d /path/to/tiny3d
```

The asset directory must contain the original `cstrike/maps/de_dust2.bsp`, its
WADs under `cstrike/` and `valve/`, `cstrike/models/player/terror/terror.mdl`,
`v_glock18.mdl`, `p_glock18.mdl`, and the Glock/magazine/slide WAVs under
`cstrike/sound/weapons/`. Source assets are only read. Generated arrays, ROMs,
logs, screenshots and emulator copies stay in ignored `build/`.

Tested dependencies:

- libdragon `494f1f586d3d6d5fc65b516a8ce29ccf42f85e15`
- Tiny3D `ec557373e986b5e041cc102a7ff787eb07921937`
- ares v148, Nintendo 64 NTSC, 8 MiB RAM

Each build writes map, model and sound reports and a SHA-256 ROM/source manifest.
SDK paths can also be supplied through `N64_INST` and `TINY3D_DIR`.

## Verification

After building, run the host tests with AddressSanitizer and UBSan:

```sh
python3 port/n64/test.py
```

They cover all 40 map spawns, 24,000 movement ticks, crouching, jumping and
landing; two-player movement and blocking; hitscan wall occlusion, damage,
reloads, death and respawn; all 4,197 source animation frames and indexed mesh
bounds. A separate comparison checks 103,239 runtime bone poses against the
original MDL data across every sequence and every source blend endpoint.

The scripted emulator cartridge uses the production simulation and renderer:

```sh
python3 port/n64/build.py --validate --smoke-test
python3 port/n64/run_ares.py --rom build/n64/cs64-dust2-2p-smoke-validation.z64
```

After 1,200 simulation ticks it prints `2P SMOKE PASS`. It exercises both views,
strafing, jump, crouch, four hits, a kill/death animation, reload and automatic
respawn, then repeats. `--smoke-test` without `--validate` measures the same route
without the deliberately slow RDP validator. Normal cartridges have no scripted
movement. Logs and exact accepted ROM identities are in `build/n64/`.

The optimized profiling build measures **34.6 FPS** in the controlled two-player
T-spawn scene, up from **19.1 FPS**. Moving combat windows measure **32.1–36.8 FPS**
with both players animated and audio active. These use emulated CPU Count and
VI framebuffer observations, not ares's approximately 60 VPS output counter.
Individual moving frames still exceed 33.3 ms; this is not a map-wide 30 FPS floor.
The scripted test and visual inspection do not establish physical-controller
reliability, input latency or real-N64 performance.

## Current limits

Performance instrumentation and reproducible benchmark commands are documented
in [port/n64/PERFORMANCE.md](port/n64/PERFORMANCE.md). The profiler measures CPU
stages, asynchronous RDP completion, actual VI framebuffer changes, and geometry
counts; the benchmark compares thirteen controlled configurations, including both model
LODs and off-screen culling. `profile_ares.py --smoke` captures moving combat;
`--validation` checks the combat and LOD/culling graphics fixtures.

This is a combat prototype, not a complete Counter-Strike match. There are no
bots, buys, bomb rules, inventory switching, burst fire, grenades, ladders,
water movement, dynamic doors or breakable objects. Damage uses one simple
player box, not per-bone hitboxes or armor. Movement, weapon accuracy and
animation transitions are approximations. World textures are 32×32, character
and weapon textures are 64×32, sky is flat color, and static lighting is sampled
into map vertex colors. Nearby models retain their source geometry and use ambient lighting. Distant
Terrorists use conservative 690/580-triangle LODs; the original 740-triangle mesh
is retained nearby. First-person animation is baked offline with a tested maximum
coordinate error of 1/16 game unit. Player aim/gait blending remains skeletal.
A strict 30 FPS floor across the whole map and physical N64 testing remain work.

See [feasibility and next milestones](port/n64/FEASIBILITY.md).

## Source repositories

The public port is maintained at [jeremybanka/cs16nd](https://github.com/jeremybanka/cs16nd).
Its original PC reference-source history comes from [nagist/cs16nd](https://github.com/nagist/cs16nd); existing source notices remain intact.
The private asset submodule is [jeremybanka/cops-and-robbers](https://github.com/jeremybanka/cops-and-robbers). Game assets and generated ROMs are not part of the public source tree.
