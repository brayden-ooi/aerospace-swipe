# Vertical multi-touch swipe → AeroSpace `focus`

**Date:** 2026-08-09
**Branch:** `feat/vertical-swipe`

## Goal

Add vertical swipe detection alongside the existing horizontal swipe. A swipe of
`fingers`-count (from config, e.g. 4) is classified by its **dominant axis**:

- **Horizontal-dominant** (`|dx| > |dy|`) → existing `workspace prev/next` (unchanged).
- **Vertical-dominant** (`|dy| > |dx|`) → new `focus up/down`.

The action is sent to AeroSpace as a **command over the existing Unix socket**
(same mechanism as horizontal), *not* as a synthesized keystroke. This mirrors the
user's AeroSpace bindings:

```
alt-k = 'focus up'    ← swipe up
alt-j = 'focus down'  ← swipe down
alt-h = 'focus left'  (horizontal already covered by workspace prev/next)
alt-l = 'focus right'
```

Finger count is shared: the same `fingers` config value gates both axes. No new
finger-count option is introduced.

## Background: how horizontal works today

1. `gestureCallback` (`main.m`) classifies swipe direction and calls `fire_gesture`.
2. `fire_gesture` picks `g_config.swipe_right` / `swipe_left` — the strings
   `"next"` / `"prev"` — and calls `switch_workspace`.
3. `switch_workspace` → `aerospace_workspace` → `execute_aerospace_command` ships a
   length-prefixed JSON frame `{"args":["workspace","next",...]}` over
   `/tmp/bobko.aerospace-$USER.sock`, with a `popen("aerospace ...")` CLI fallback.

The current state machine is x-axis only. Vertical motion (`dy`) is used *solely*
to reject/cancel a gesture (`main.m:142`, `main.m:157`). `TouchConverter` computes
only `velocity_x` (`event_tap.m:33-51`).

## Changes by file

### `event_tap.h`
- `touch`: rename `velocity` → `velocity_x`; add `double velocity_y`.
- `gesture_ctx`: add
  - `int axis;` — `0` undecided, `1` horizontal, `2` vertical.
  - `float peak_vely;`
  - `float prev_y[MAX_TOUCHES], base_y[MAX_TOUCHES];` (mirror the existing x arrays).

### `event_tap.m` (`convert_nstouch`)
- `touch_state` already stores `y`. Compute `velocity_y = dy/dt` exactly as
  `velocity_x` is computed, and set `nt.velocity_y`.

### `config.h`
- Add `const char* swipe_up;` / `const char* swipe_down;` (values `"up"` / `"down"`).
- Derive from `natural_swipe`, consistent with `swipe_left/right`:
  - `natural_swipe = true`  → `swipe_up = "up"`,   `swipe_down = "down"` (direct).
  - `natural_swipe = false` → `swipe_up = "down"`, `swipe_down = "up"` (inverted).

### `aerospace.h` / `aerospace.c`
- Add `char* aerospace_focus(aerospace* client, const char* direction);`
  → sends `["focus", direction]` via `execute_aerospace_command`
  (no `--wrap-around`, no stdin flag). Reuses socket + CLI fallback.

### `main.m`
- `calculate_touch_averages`: also compute average y-velocity (`avg_vel_y`).
- Extract a **pure axis-decision helper** (e.g. `decide_axis(dx, dy, velx, vely, fast)`)
  so the classification logic is reviewable in isolation.
- `handle_idle_state`: when arm conditions met, set `ctx->axis` by dominant axis and
  seed the matching start/peak fields (x or y).
- `handle_armed_state`: operate on the armed axis's delta/velocity; if the *other*
  axis overtakes it, reset (generalizes the current cross-axis reject).
- `fire_gesture`: route by `ctx->axis`:
  - horizontal → `switch_workspace(swipe_left/right)`.
  - vertical   → `switch_focus(swipe_up/down)`.
- `handle_committed_state`: reversal check operates on the committed axis.
- Add `switch_focus(const char* dir)` mirroring `switch_workspace` but calling
  `aerospace_focus` (no workspace-list / wrap logic; reuse haptic-on-success).

## Data flow

```
touch (velocity_x, velocity_y)
  → gestureCallback
  → calculate_touch_averages (avg_x/y position, avg vel x/y)
  → GS_IDLE: decide_axis → set ctx->axis, arm
  → GS_ARMED: track committed axis; reset if opposite axis overtakes
  → fire_gesture: route
       horizontal → switch_workspace → aerospace_workspace  (workspace prev/next)
       vertical   → switch_focus     → aerospace_focus       (focus up/down)
  → socket frame to AeroSpace (CLI fallback if socket down)
```

## Edge cases

- **Diagonal swipes:** dominant axis at arm time wins. If dominance flips while armed,
  reset to avoid a wrong-axis fire.
- **Committed reversal:** only re-arms within the same committed axis.
- **`natural_swipe`:** inverts vertical mapping the same way it inverts horizontal.
- **Palm rejection, tolerance, fast/slow thresholds:** unchanged; the same knobs apply
  to whichever axis is armed.

## Testing / verification

The repo has no test harness and gesture code requires a physical trackpad, so:

1. `make` builds clean (no warnings introduced).
2. `make restart`, then manually verify with `fingers: 4`:
   - 4-finger horizontal swipe still switches workspaces (prev/next).
   - 4-finger vertical swipe up → focus up; down → focus down.
3. The extracted `decide_axis` helper is pure and reviewable without CoreGraphics.

## Out of scope (YAGNI)

- Keystroke synthesis (CGEvent). Not needed — AeroSpace commands cover the use case.
- Separate finger counts per axis.
- Arbitrary user-configurable command strings for vertical (fixed to `focus up/down`).
