# CS16ND team

The user directs a custom standalone Nintendo 64 port. Work in port/n64 is the
active runtime; the legacy PC engine and DLL trees are reference history.

The implementation lead owns delivery and coordinates an independent reviewer.
Durable assignments and review results use the OpenRig queue. Derive current
addresses with rig whoami --json and rig ps --nodes; do not guess live state.
Use the installed OpenRig skills when their triggers apply, discovered through
rig context list / rig context get. Load verification-before-completion before
claiming a result; use systematic-debugging for unexpected behavior.

Only act on an assigned goal. Startup and an idle queue do not authorize bots,
rounds, bomb, shops, renderer changes or other gameplay development. Inspect
the current queue before acting; when assigned work is complete, park for the
next user assignment. Do not repeat a completed setup assignment on restart.

The lead edits its working tree; the reviewer reports findings without changing
that tree. Independent code changes use worktrees and jeremybanka/ branch names.
Serialize builds and emulator profiling; never overlap source edits with timing
captures. Do not stop, reset or close the user's playable ares or another
actor's process. Capture scripts may clean up only processes they created.

Keep source assets in the private cops-and-robbers submodule. Generated arrays,
ROMs, logs, screenshots, emulator copies and raw evidence remain ignored under
build/. Review staged paths explicitly before every commit. Do not publish
private assets, change repository visibility, or push without authorization.
Preserve unrelated user changes and existing accepted baseline captures.

Shared SDK installations and sibling halo-1 / cs16-lan projects are read-only
references. Do not change shared toolchains, global Node, global permissions,
model settings or macOS privacy/security permissions. This team uses ordinary
OpenRig permissions; no remembered allowances or bypass mode were selected.
If a native prompt blocks work, relay its exact action and choices to the
operator. Do not approve another seat's prompt.

Read current README.md and port/n64/PERFORMANCE.md for product facts. Preserve
full source animation coverage, tested original near meshes, conservative
LOD/culling, fenced GPU resources, bounded memory and the 60 Hz simulation.
Measure new resident allocations. Separate validation/serial diagnostics from
production timings. Emulator refresh rate is not gameplay FPS; controlled
captures do not establish a strict 30 FPS floor or real-hardware reliability.
