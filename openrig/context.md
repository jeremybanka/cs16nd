# Project grounding

Confirm your live identity with rig whoami --json and your actual working
folder. The source project is cs16nd. Read README.md, port/n64/PERFORMANCE.md,
and port/n64/FEASIBILITY.md; current runtime documentation takes precedence over stale
feasibility prose. If a project.yaml is declared, use rig context work-install
for the project's selected context.

At initial setup (2026-10-27), the user requested OpenRig configuration and a
check-in on main, followed by parking ready. That history is not a standing
assignment. Read rig queue list --owned for current work and its closure state.
Do not begin feature work or repeat setup on startup. Report role/context
readiness to the operator with a queue row; with no active assignment, park.
The reviewer waits for the lead's concrete review handoff.

The current prototype has two-player horizontal splitscreen Dust II,
Terrorist/Phoenix characters, Glock combat, full source animations, death and
respawn, and active audio in 8 MiB. The accepted controlled near capture is
34.62 FPS (previously 19.12); moving combat windows are 32.09–36.80 FPS with a
38.38 ms worst frame. These are historical measured scenes, not a map-wide or
hardware guarantee. Read the current performance document for evidence and
remaining gaps: SDK __rspq_syncpoints_done diagnostics, representative routes,
physical controllers and real N64. Free heap was 670,952 bytes after the tested
additional scenes; remeasure rather than treating that as a budget.

Build and test commands are in README.md and port/n64/PERFORMANCE.md. Use unique
ignored capture names and retain matching ROM/source manifests. No new build,
benchmark or emulator run is needed just to configure the team. If later work
requires one, coordinate exclusive access and preserve prior accepted evidence.
