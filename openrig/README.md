# CS16ND OpenRig team

This directory contains the reusable configuration for a two-seat Codex team:
`dev.build` owns implementation and delivery; `dev.review` independently reviews
the lead's concrete candidate without editing its working tree. The team works
on the standalone runtime in `port/n64/`. Starting it does not assign gameplay
work. After an assigned outcome is delivered, both seats park ready.

## Requirements and configuration

The setup was validated with OpenRig 0.7.3 and Node 22.23.4. The repository's
`mise.toml` selects that Node version when using mise; the N64 toolchain still
uses Python and its existing SDKs. OpenRig and a working Codex login must already
be installed. The agent specs use the installed
`openrig-home:plugins/openrig-core` plugin (OpenRig 0.6.5 or later).

Both seats inherit Codex model settings. The spec selects ordinary OpenRig
permissions, with no permission policy, model pin, native configuration fragment,
extra provider or remembered command allowances. Inspect the launch plan and
native prompts; the intended posture is `workspace-write` with `on-request`.
Do not change global settings or privacy permissions to get through a prompt.

`rig.yaml` resolves local agent specs and startup files relative to this directory.
Each member's `cwd: ".."` resolves to the repository. Roles are sent separately
to each seat; team culture is shared. Generated root `AGENTS.md`, `.openrig/`
state and the `.codex/plugins/openrig-core/` projection are ignored. Edit the
authored files here, not those generated copies.

The root `project.yaml` selects the project's current documentation. An operator
registers the checkout as project `cs16nd` with `rigs: [cs16nd]` in the instance's
workspace catalog; its machine-specific root path stays outside this repository.
Check `rig context work-install --json` from a seat: it should select `cs16nd`
and this checkout. The manifest does not install additional skills or create
missions; agents discover the installed skill library as needed.

## Start and inspect

Run these from the repository root to validate a new checkout:

```sh
rig spec validate openrig/rig.yaml
rig agent validate openrig/agents/build/agent.yaml
rig agent validate openrig/agents/review/agent.yaml
rig spec audit openrig/rig.yaml
rig spec preflight openrig/rig.yaml
rig up openrig/rig.yaml --plan
```

For a **new team**, after reviewing the plan:

```sh
rig up openrig/rig.yaml
rig ps --nodes --rig cs16nd
```

Check each seat's startup status and readiness reply. A valid spec or successful
launch command alone does not establish that the agents are ready. On the host
where initial setup already launched the team, use status and the existing seats.
Keep that host's bootstrap source: existing seats retain saved startup paths,
and adding this portable copy does not retarget them.

## Give the next task

Tell the kernel operator the desired outcome for `cs16nd`, or address the lead
in its existing conversation. Include what success looks like and any limits;
the lead routes independent review and returns evidence. For a durable CLI
assignment, first derive the lead's address from `rig ps --nodes --rig cs16nd`,
then send a task file to that address (the declared default is shown here):

```sh
rig queue create --destination dev-build@cs16nd \
  --summary 'Describe the requested outcome' --tags project:cs16nd \
  --body-file /path/to/task.txt
```

The lead claims current queued work. A restored conversation must not treat the
historical setup request as a new task. Builds and emulator captures require an
assigned outcome, exclusive access and unique ignored evidence paths. Setup
validation itself needs no ROM build or emulator run.

## Stop and resume

Parking means leaving the conversations available and idle. To stop the team
deliberately, check for active work first, then run:

```sh
rig ps --nodes --rig cs16nd
rig down cs16nd
```

Stopping ends both live sessions and stops their work. OpenRig attempts a snapshot
first. To bring the stopped team back with its existing conversations:

```sh
rig up cs16nd --existing
rig ps --nodes --rig cs16nd
```

Resume depends on the saved snapshot and native harness continuity; inspect the
reported outcome. Do not use a fresh spec launch as normal resume: it replaces
a stopped team with a fresh generation. Do not use `--delete` if you intend to
resume. No stop or relaunch is necessary just to leave the team ready for a task.
