# CLAUDE.md — aerospace-swipe (fork)

## Fork & remotes (IMPORTANT for future agents)

This is a **fork** of the upstream project. Git remotes are wired as:

- `origin`   → `https://github.com/brayden-ooi/aerospace-swipe.git`  (our fork — push here)
- `upstream` → `https://github.com/acsandmann/aerospace-swipe.git`   (original — pull updates from here)

So:
- `git push` goes to **our fork** (`origin`), not the upstream.
- To sync with the original author: `git fetch upstream && git merge upstream/main` (or rebase).
- Open PRs against `brayden-ooi/aerospace-swipe` unless explicitly contributing back upstream.

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
