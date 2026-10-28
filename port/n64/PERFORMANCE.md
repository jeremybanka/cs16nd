# Performance instrumentation

The main cost added by the two-player combat slice is CPU work to update the
models. On the controlled T-spawn capture, two map-only views run at **52.5 FPS**;
the complete scene runs at **19.1 FPS**. Keeping the models visible but freezing
their vertex updates reaches **41.2 FPS**. This identifies model updates as the
first optimization target, with map traversal/submission next.

These are ares v148 measurements from the emulated CPU Count timer. They are not
host wall-clock timings or real-console results. The original single-player
cartridge is not being recompiled by this benchmark: the fullscreen map-only
case is a comparable configuration of the current runtime, which still simulates
two players. It reproduces the approximately 60 FPS baseline at this location.

## Run it

Build and capture the controlled benchmark:

```sh
python3 port/n64/build.py --benchmark
python3 port/n64/profile_ares.py
```

The capture script launches ares with the project's input profile, records one
complete cycle, terminates only its own process group, and writes:

- `build/n64/perf-report.log`: original emulator output.
- `build/n64/perf-report.json`: timing windows, counters, ROM/source manifest and log hash.
- `build/n64/perf-report.csv`: flat columns for comparison with future captures.
- `build/n64/perf-report.md`: readable timing tables and interpretation notes.

The existing capture from development uses `build/n64/ares-bench.log` as its raw
log. Preserve a baseline with `--output build/n64/perf-before` when running the
capture script. It defaults to a 600-second host timeout. Do not run multiple
copies of the benchmark simultaneously.

For interactive exploration instead of scripted configurations:

```sh
python3 port/n64/build.py --profile
python3 port/n64/run_ares.py --rom build/n64/cs64-dust2-2p-profile.z64 \
  > build/n64/perf-live.log 2>&1
# Stop the emulator before summarizing the complete log:
python3 port/n64/profile_report.py build/n64/perf-live.log \
  --output build/n64/perf-live \
  --manifest build/n64/cs64-dust2-2p-profile-manifest.json
```

`--profile --smoke-test` profiles the existing moving combat fixture. Benchmark
and smoke-test are mutually exclusive. `--validate` cannot be combined with
profiling: the RDP validator deliberately slows execution and is not a baseline.
A normal build compiles out all timing scopes, counters, completion wrappers,
and benchmark behavior.

## What is measured

Each window discards 36 warm-up frames, then measures 120 frames. Benchmark
transitions reset and settle both players at the same original T spawns, use
neutral controls and preserve the same map, camera directions and renderer.
The final full-scene repeat detects drift. Normal input remains active in a live
profile build. Warm-up and report printing are outside the sample window.

CPU scopes partition elapsed frame time into input, simulation, audio events,
RSP buffer reuse waits, pose evaluation for each asset, bone matrices, skinning,
vertex cache writeback, model transforms, display acquisition, camera/setup,
PVS expansion, per-view map culling/submission, actor/weapon submission, HUD and
presentation submission. A residual `other` bucket accounts for unspecific work
and instrumentation. Runtime and report-parser assertions reject overlapping
CPU totals. p50, p95 and maximum frame durations are also recorded.

Skinning has nested detail timers for vertex transforms versus packing, and for
the character, held Glock and first-person Glock. These details overlap the main
`skinning` scope; **do not add them to the frame total**. Counters record simulation
steps, submitted faces/triangles/vertices, texture uploads, unique skinned
vertices, packed vertices and animation keyframe cache hits/misses.

The RDP completion callback timestamps the existing detach/completion boundary
and then calls `display_show`, preserving asynchronous presentation. It reports:

- `rdp_frame_latency_ms`: graphics submission start to the completion interrupt.
  This includes CPU command generation and queued work, not just GPU execution.
- `rdp_queue_tail_ms`: final detach submission to the completion interrupt. This
  can overlap the CPU's next frame; it is not an additional CPU bucket.
- `vi_hz`: observed vertical interrupts, about 59.8–59.9 Hz here.
- `vi_buffer_changes_hz`: observed changes of the VI framebuffer origin, about
  19.1/s for the complete scene. This distinguishes new frames from repeated
  display refreshes. The observer can trail libdragon by one VI interrupt.

Audio callback duration/count are measured separately. Interrupt time is already
included in whichever CPU scope it interrupts; do not add it a second time.
At the window boundary only, the profiler drains outstanding work before reusing
callback storage and printing a report. It adds no per-frame RSP/RDP drain in the
normal profiling mode. Counter wrap uses unsigned deltas; individual samples and
windows must be shorter than the approximately 92-second Count wrap period.

## Captured results

Each row is one 120-frame window after warm-up. Removing a component also skips
its unused CPU pose/skinning work; freezing keeps the already-prepared meshes,
textures and draw commands on screen.

| Configuration | FPS | Mean frame ms |
| --- | ---: | ---: |
| One map-only view, fullscreen | 59.83 | 16.71 |
| One map-only view, half-screen | 59.83 | 16.71 |
| Two map-only views | 52.51 | 19.04 |
| Two full views, frozen models | 41.15 | 24.29 |
| Two views, no first-person hands/Glocks | 27.90 | 35.83 |
| Two views, no third-person characters/held Glocks | 30.29 | 33.01 |
| Two full views | 19.12 | 52.28 |
| Two full views, no HUD | 19.48 | 51.32 |
| Full-scene repeat | 19.10 | 52.34 |

Full-scene CPU breakdown:

| Work | Mean ms/frame |
| --- | ---: |
| Vertex packing, rounding, UV/color stores (nested within skinning) | 13.83 |
| Unique vertex transforms (nested within skinning) | 5.17 |
| Whole skinning scope | 19.09 |
| Player/weapon pose evaluation | 4.39 |
| Bone matrices | 2.36 |
| Vertex writeback and actor transforms | 0.86 |
| **All model CPU work** | **26.69** |
| PVS + map culling/submission, both views | 19.02 |
| Simulation (3.13 fixed ticks per rendered frame) | 4.46 |
| HUD | 1.10 |
| Display + RSP buffer reuse waits | 0.014 |

The full scene submits 1,376 map triangles and 3,400 model triangles per frame.
CPU skinning transforms 1,754 unique model vertices, then repacks 4,456 vertex
records. The packing loop currently makes **13,368 `lroundf` calls per frame**
(three per packed vertex); the linked toolchain uses a general software routine
for this conversion. Packing time also includes indexing and attribute stores,
so the full 13.83 ms cannot be attributed to `lroundf` alone. The settled animation
cache is already about 95% hits; frame decoding is not the dominant idle cost.

The graphics-completion latency is 26.0 ms and queue tail is 5.7 ms in the full
scene. The negligible explicit buffer waits do not mean graphics are free:
map submission can block on command queues. The two views submit the same map
geometry with or without characters, yet their CPU submission time increases
when model commands are interleaved.

## Serialized diagnostic

To distinguish queue overlap from pass costs:

```sh
python3 port/n64/build.py --benchmark --profile-serial
python3 port/n64/profile_ares.py --serial
```

This adds `rspq_wait()` after major passes. It **destroys normal CPU/RSP/RDP
overlap** and is diagnostic only. Do not compare its FPS as a production result
or add its drain times to the pipelined frame totals.

The captured full scene falls to 15.94 FPS with these barriers. After CPU
submission, actor passes require about 5.66 ms of combined drain time across both
views; first-person weapon passes require about 6.25 ms. Map CPU work falls to
15.42 ms from 19.02 ms because interleaved model work is drained explicitly.
This is evidence of graphics queue backpressure as well as CPU model cost.

## Next optimization experiments

1. Quantize transformed positions once per unique vertex, then reuse them when
   packing corners. Investigate a bounded fast float-to-int conversion while
   preserving rounding behavior, and initialize immutable UV/color fields once.
   Use the existing source-pose and sanitizer tests to guard fidelity.
2. Cache visible face candidates / draw batches instead of scanning the entire
   material-sorted face list for each view. Keep PVS and exact clipping intact.
3. After measuring those changes, consider lower model update rates or moving
   bone/vertex work onto the RSP. Keep idle cache hit rates and moving combat
   captures separate; the stationary benchmark does not represent every state.

No such performance optimization is included in this instrumentation change.
Removing 14 ms alone would still leave roughly 38 ms/frame in this capture;
30 FPS is not guaranteed by one change.

## Findings outside the main bottleneck and validation

The capture records **zero audio callbacks**. Inspection of the pinned SDK shows
that registering the callback does not prime the audio DMA queue, and the current
`sound_init` only registers it. Thus the earlier build queues weapon sounds in
software but does not actually start playback. The silent ablation is not a valid
estimate of an active mixer's cost. This playback bug is documented here and is
left separate from the timing instrumentation so the regression baseline remains
comparable.

Ares emitted two cache-coherency diagnostics in each benchmark capture. The
reported address resolves to libdragon's `__rspq_syncpoints_done`, with CPU PCs
in `rspq_sp_interrupt` / `rspq_syncpoint_check`, rather than model vertex buffers.
These SDK synchronization diagnostics are retained in the raw logs and report
metadata; they have not been fixed or dismissed as harmless. The normal
non-profiled two-player smoke test after this refactor passed 1,200 ticks with
**zero RDP validation messages and zero cache-coherency messages**.

Host ASan/UBSan movement, combat and full animation tests still pass, as do the
103,239 source-pose comparisons. Report tests reject overlapping scopes,
inconsistent FPS, and truncated/missing captures. The capture runner was exercised
end-to-end on the serialized benchmark and cleaned up its own emulator process.
Instrumentation and code layout can perturb timing by a few percent; compare
matching instrumented builds and use the repeated baseline. Real N64 profiling
and worst-case routes through mid, long A, B and tunnels remain necessary.
