# CLAUDE.md — aerospace-swipe (fork)

## Fork & remotes (IMPORTANT for future agents)

This is a **fork** of the upstream project. Git remotes are wired as:

- `origin`   → `https://github.com/brayden-ooi/aerospace-swipe.git`  (our fork — push here)
- `upstream` → `https://github.com/acsandmann/aerospace-swipe.git`   (original — pull updates from here)

So:
- `git push` goes to **our fork** (`origin`), not the upstream.
- We do **not** intend to contribute changes back upstream. `upstream` is pull-only.

## Git flow (branching model — IMPORTANT for future agents)

This fork accumulates personal customizations. The branches have distinct roles:

- **`main`** — a clean mirror of `upstream/main`. NEVER commit to it directly.
  Only fast-forward it from upstream, then push to `origin/main`:
  ```bash
  git fetch upstream
  git checkout main && git merge --ff-only upstream/main && git push origin main
  ```
- **`local`** — the daily-driver / permanently-divergent branch. This is what
  gets checked out and built (`make install` / `make restart` build whatever is
  checked out). It is the **aggregation** of `main` + every feature/customization.
  Never rebase it (it's long-lived and drives the launch agent); always **merge**.
  `local` is never pushed anywhere for upstreaming — it's ours.
- **`feat/*` (and other customization branches)** — every change starts on its own
  branch cut **from `main`**, not from `local`. Keeps each change isolated and
  reviewable.

Integration rule — **everything reaches `local` via a PR/merge, never direct commits**:
- A finished `feat/*` branch → merge into `local`.
- When `main` is updated from upstream → merge `main` into `local` too (via PR).

```
upstream/main ──► main (mirror, ff-only, pushed to origin/main)
                   ├── feat/vertical-swipe ──┐
                   ├── feat/next-thing      ─┤   (each cut from main)
                   └───────────► local ◄─────┘   (daily driver = main + all features)
```

Practical notes:
- Start new work: `git checkout main && git checkout -b feat/<name>`.
- Ship it: open a PR merging `feat/<name>` → `local` (on `origin`), then merge.
- Purely personal tweaks too small for a branch may be committed on `local` directly.

## Active work

- Branch `feat/vertical-swipe`: adding vertical multi-touch swipe support.
  - Design spec: `docs/superpowers/specs/2026-08-09-vertical-swipe-design.md`.
  - Summary: the single `fingers`-count gesture is classified by dominant axis —
    horizontal → `workspace prev/next` (existing), vertical → `focus up/down` (new).
  - Actions are sent as **AeroSpace commands over the Unix socket**
    (`/tmp/bobko.aerospace-$USER.sock`), NOT synthesized keystrokes — consistent with
    the existing horizontal implementation.

## Architecture quick map

- `src/event_tap.m` / `.h` — CGEvent gesture tap; `TouchConverter` normalizes NSTouch
  into `touch` structs (position, phase, per-axis velocity).
- `src/main.m` — gesture state machine (`GS_IDLE` → `GS_ARMED` → `GS_COMMITTED`) and
  action dispatch (`switch_workspace`, and new `switch_focus`).
- `src/aerospace.c` / `.h` — Unix-socket client to AeroSpace with CLI (`popen`) fallback.
- `src/config.h` — JSON config loader (`~/.config/aerospace-swipe/config.json`).
- `src/haptic.c` — optional haptic feedback.

## Build / run

- `make` — build.
- `make install` — install launchd service.
- `make restart` — reload the launch agent after config/code changes.
