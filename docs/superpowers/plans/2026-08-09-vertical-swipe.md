# Vertical Multi-Touch Swipe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a vertical multi-touch swipe that sends AeroSpace `focus up`/`focus down` over the existing socket, while leaving the existing horizontal `workspace prev/next` behavior unchanged.

**Architecture:** The single `fingers`-count gesture is classified by its dominant axis at arm time. Horizontal-dominant swipes keep firing `workspace prev/next`; vertical-dominant swipes fire `focus up/down`. Actions are sent as AeroSpace commands over the Unix socket (`/tmp/bobko.aerospace-$USER.sock`), never as synthesized keystrokes — identical transport to the horizontal path.

**Tech Stack:** C99 / Objective-C (ARC), CoreGraphics event tap, MultitouchSupport, yyjson, `make`/clang.

## Global Constraints

- Language/build: C99 + Objective-C ARC, compiled via the existing `makefile` (`make` must build clean with `-Wall -Wextra`, no new warnings).
- Horizontal swipe behavior MUST remain byte-for-byte identical: a horizontal-dominant `fingers`-count swipe still fires `workspace prev/next`.
- Finger count is shared across axes via the existing `fingers` config value. Do NOT add a per-axis finger-count option.
- Vertical actions are AeroSpace commands over the socket (with the existing CLI `popen` fallback). No CGEvent keystroke synthesis.
- Coordinate convention: `NSTouch normalizedPosition` has origin bottom-left, so **y increases upward** → positive y-velocity means a physical **up** swipe.
- `natural_swipe` mapping for vertical: `true` → up=`focus up`, down=`focus down`; `false` → inverted.
- Follow existing code style (tabs for indentation, `clang-format` config in `.clang-format`).

**Deviation from spec:** To keep every task independently compilable, we do NOT rename the existing `touch.velocity` field. We keep `velocity` meaning x-velocity and ADD a new `velocity_y` field. Behavior is identical to the spec's intent.

---

### Task 1: Pure axis-decision helper (`decide_axis`)

Extract the horizontal-vs-vertical classification into a pure, unit-testable function. This is the one piece of logic we can test without a trackpad.

**Files:**
- Create: `src/gesture_axis.h`
- Create: `src/gesture_axis.c`
- Create: `tests/test_gesture_axis.c`
- Modify: `makefile` (add `src/gesture_axis.c` to `SRC_FILES`; add a `test` target)

**Interfaces:**
- Produces:
  - `typedef enum { AXIS_NONE = 0, AXIS_HORIZONTAL = 1, AXIS_VERTICAL = 2 } gesture_axis;`
  - `gesture_axis decide_axis(float dx, float dy, bool fast, float activate_pct);`

- [ ] **Step 1: Write the failing test**

Create `tests/test_gesture_axis.c`:

```c
#include "../src/gesture_axis.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	const float A = 0.05f; // ACTIVATE_PCT

	// Dominant axis past threshold (slow swipe)
	assert(decide_axis(0.10f, 0.01f, false, A) == AXIS_HORIZONTAL);
	assert(decide_axis(0.01f, 0.10f, false, A) == AXIS_VERTICAL);

	// Magnitude, not sign, decides dominance
	assert(decide_axis(-0.10f, 0.01f, false, A) == AXIS_HORIZONTAL);
	assert(decide_axis(0.01f, -0.10f, false, A) == AXIS_VERTICAL);

	// Below threshold + slow -> no arm
	assert(decide_axis(0.02f, 0.01f, false, A) == AXIS_NONE);
	assert(decide_axis(0.01f, 0.02f, false, A) == AXIS_NONE);

	// Fast arms even below threshold; dominant axis still wins
	assert(decide_axis(0.02f, 0.01f, true, A) == AXIS_HORIZONTAL);
	assert(decide_axis(0.01f, 0.02f, true, A) == AXIS_VERTICAL);

	// Exact tie favors horizontal (>=), when past threshold
	assert(decide_axis(0.05f, 0.05f, false, A) == AXIS_HORIZONTAL);

	printf("All decide_axis tests passed.\n");
	return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `clang -std=c99 -O0 -g -Wall -Wextra -o /tmp/test_gesture_axis src/gesture_axis.c tests/test_gesture_axis.c 2>&1 | head`
Expected: FAIL — compile error, `src/gesture_axis.h` / `src/gesture_axis.c` do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `src/gesture_axis.h`:

```c
#pragma once
#include <stdbool.h>

typedef enum {
	AXIS_NONE = 0,
	AXIS_HORIZONTAL = 1,
	AXIS_VERTICAL = 2
} gesture_axis;

// Decide which axis a gesture should arm on, given accumulated average
// displacement on each axis and whether the swipe is currently "fast".
// The dominant axis (larger |displacement|) is chosen; it arms only if the
// swipe is fast OR the dominant displacement has crossed activate_pct.
// Ties (|dx| == |dy|) favor horizontal. Returns AXIS_NONE if neither arms.
gesture_axis decide_axis(float dx, float dy, bool fast, float activate_pct);
```

Create `src/gesture_axis.c`:

```c
#include "gesture_axis.h"
#include <math.h>

gesture_axis decide_axis(float dx, float dy, bool fast, float activate_pct)
{
	float adx = fabsf(dx);
	float ady = fabsf(dy);

	if (adx >= ady) {
		if (fast || adx >= activate_pct)
			return AXIS_HORIZONTAL;
	} else {
		if (fast || ady >= activate_pct)
			return AXIS_VERTICAL;
	}
	return AXIS_NONE;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `clang -std=c99 -O0 -g -Wall -Wextra -o /tmp/test_gesture_axis src/gesture_axis.c tests/test_gesture_axis.c && /tmp/test_gesture_axis`
Expected: PASS — prints `All decide_axis tests passed.`

- [ ] **Step 5: Wire into the build**

In `makefile`, add `src/gesture_axis.c` to `SRC_FILES` (line 11):

```make
SRC_FILES = src/aerospace.c src/yyjson.c src/haptic.c src/gesture_axis.c src/event_tap.m src/main.m
```

Add a `test` target (near the `format` target, ~line 92) and add `test` to `.PHONY` (line 22):

```make
test:
	$(CC) -std=c99 -O0 -g -Wall -Wextra -o /tmp/test_gesture_axis src/gesture_axis.c tests/test_gesture_axis.c
	/tmp/test_gesture_axis
```

- [ ] **Step 6: Verify build + test targets both work**

Run: `make test && make`
Expected: test prints pass line; `make` produces the `swipe` binary with no new warnings.

- [ ] **Step 7: Commit**

```bash
git add src/gesture_axis.h src/gesture_axis.c tests/test_gesture_axis.c makefile
git commit -m "feat: add pure decide_axis gesture classifier with unit test"
```

---

### Task 2: AeroSpace `focus` command sender

Add a socket command that sends `focus up`/`focus down`, mirroring `aerospace_workspace`.

**Files:**
- Modify: `src/aerospace.h` (add declaration after line 17)
- Modify: `src/aerospace.c` (add function near `aerospace_switch`, ~line 455)

**Interfaces:**
- Consumes: `execute_aerospace_command(aerospace*, const char** args, int arg_count, const char* stdin_payload, const char* expected_output_field)` (static, in `aerospace.c`).
- Produces: `char* aerospace_focus(aerospace* client, const char* direction);` — sends `["focus", direction]`; returns `NULL` on success, or a heap-allocated error string on failure (caller frees). Same contract as `aerospace_switch`.

- [ ] **Step 1: Declare the function**

In `src/aerospace.h`, add after `aerospace_switch` (line 15):

```c
char* aerospace_focus(aerospace* client, const char* direction);
```

- [ ] **Step 2: Implement it**

In `src/aerospace.c`, add after `aerospace_switch` (after line 458):

```c
char* aerospace_focus(aerospace* client, const char* direction)
{
	const char* args[2] = { "focus", direction };
	return execute_aerospace_command(client, args, 2, "", NULL);
}
```

- [ ] **Step 3: Verify it builds**

Run: `make`
Expected: PASS — `swipe` binary builds, no new warnings. (No unit test: this needs a live AeroSpace socket; it is exercised in Task 6 manual verification.)

- [ ] **Step 4: Commit**

```bash
git add src/aerospace.h src/aerospace.c
git commit -m "feat: add aerospace_focus socket command sender"
```

---

### Task 3: Capture vertical velocity in the touch pipeline

Add per-touch vertical velocity so the state machine can measure the y axis. Additive only — nothing else references the new field yet, so this compiles standalone.

**Files:**
- Modify: `src/event_tap.h` (`touch` struct, lines 23-30)
- Modify: `src/event_tap.m` (`convert_nstouch`, lines 33-51)

**Interfaces:**
- Produces: `touch.velocity_y` (double) — average per-touch vertical velocity in normalized units/sec, computed identically to the existing `touch.velocity` (x).

- [ ] **Step 1: Add the struct field**

In `src/event_tap.h`, change the `touch` struct (lines 23-30) to:

```c
typedef struct {
	double x;
	double y;
	int phase;
	double timestamp;
	double velocity;   // x-axis velocity (unchanged meaning)
	double velocity_y; // y-axis velocity
	bool is_palm;
} touch;
```

- [ ] **Step 2: Compute velocity_y in convert_nstouch**

In `src/event_tap.m`, replace the velocity block (lines 33-51) with:

```c
	double velocity_x = 0.0;
	double velocity_y = 0.0;
	touch_state* state = (touch_state*)CFDictionaryGetValue(touchStates, (__bridge const void*)(touchIdentity));
	if (state) {
		double dt = nt.timestamp - state->timestamp;
		if (dt > 0) {
			velocity_x = (nt.x - state->x) / dt;
			velocity_y = (nt.y - state->y) / dt;
		}
		state->x = nt.x;
		state->y = nt.y;
		state->timestamp = nt.timestamp;
	} else {
		state = malloc(sizeof(touch_state));
		if (state) {
			state->x = nt.x;
			state->y = nt.y;
			state->timestamp = nt.timestamp;
			CFDictionarySetValue(touchStates, (__bridge const void*)(touchIdentity), state);
		}
	}
	nt.velocity = velocity_x;
	nt.velocity_y = velocity_y;
```

- [ ] **Step 3: Verify it builds**

Run: `make`
Expected: PASS — no new warnings. `velocity_y` is set but not yet read; that is expected.

- [ ] **Step 4: Commit**

```bash
git add src/event_tap.h src/event_tap.m
git commit -m "feat: capture per-touch vertical velocity"
```

---

### Task 4: Config `swipe_up` / `swipe_down`

Add the vertical action strings, derived from `natural_swipe`, mirroring `swipe_left`/`swipe_right`. Additive only.

**Files:**
- Modify: `src/config.h` (`Config` struct lines 29-30; `default_config` line 52-53; `load_config` derivation lines 156-157)

**Interfaces:**
- Produces: `Config.swipe_up`, `Config.swipe_down` (`const char*`), each `"up"` or `"down"`.

- [ ] **Step 1: Add struct fields**

In `src/config.h`, in the `Config` struct after `swipe_right` (line 30), add:

```c
	const char* swipe_up;
	const char* swipe_down;
```

- [ ] **Step 2: Set defaults**

In `default_config()` after the `swipe_right` assignment (line 53), add:

```c
	config.swipe_up = "up";
	config.swipe_down = "down";
```

- [ ] **Step 3: Derive from natural_swipe**

In `load_config()`, replace the existing derivation block (lines 156-157) with:

```c
	config.swipe_left = config.natural_swipe ? "next" : "prev";
	config.swipe_right = config.natural_swipe ? "prev" : "next";
	config.swipe_up = config.natural_swipe ? "up" : "down";
	config.swipe_down = config.natural_swipe ? "down" : "up";
```

- [ ] **Step 4: Verify it builds**

Run: `make`
Expected: PASS — no new warnings. New fields set but not yet read; expected.

- [ ] **Step 5: Commit**

```bash
git add src/config.h
git commit -m "feat: add swipe_up/swipe_down config derived from natural_swipe"
```

---

### Task 5: Wire axis-aware detection and dispatch

The core change: `gesture_ctx` gains y-axis tracking; the state machine classifies by axis via `decide_axis` and routes to `switch_workspace` (horizontal) or a new `switch_focus` (vertical).

**Files:**
- Modify: `src/event_tap.h` (`gesture_ctx`, lines 46-51)
- Modify: `src/main.m` (include; `switch_focus`; `reset_gesture_state`; `fire_gesture`; `calculate_touch_averages`; `handle_committed_state`; `handle_idle_state`; `handle_armed_state`; `gestureCallback`)

**Interfaces:**
- Consumes: `decide_axis`/`AXIS_*` (Task 1), `aerospace_focus` (Task 2), `touch.velocity_y` (Task 3), `g_config.swipe_up`/`swipe_down` (Task 4).
- Produces: axis-routed gesture firing. `gesture_ctx.axis` (int, holds a `gesture_axis` value), `gesture_ctx.peak_vely`, `gesture_ctx.prev_y[]`, `gesture_ctx.base_y[]`.

- [ ] **Step 1: Extend gesture_ctx**

In `src/event_tap.h`, replace the `gesture_ctx` struct (lines 46-51) with:

```c
typedef struct {
	gesture_state state;
	int axis; // gesture_axis: 0=none, 1=horizontal, 2=vertical
	float start_x, start_y, peak_velx, peak_vely;
	int dir, last_fire_dir;
	float prev_x[MAX_TOUCHES], base_x[MAX_TOUCHES];
	float prev_y[MAX_TOUCHES], base_y[MAX_TOUCHES];
} gesture_ctx;
```

- [ ] **Step 2: Include the axis helper**

In `src/main.m`, add with the other includes (after line 6, `#include "haptic.h"`):

```c
#include "gesture_axis.h"
```

- [ ] **Step 3: Add switch_focus and reset axis**

In `src/main.m`, immediately after `switch_workspace` (after line 46), add:

```c
static void switch_focus(const char* dir)
{
	char* result = aerospace_focus(g_aerospace, dir);
	if (result) {
		fprintf(stderr, "Error: Failed to focus '%s': %s\n", dir, result);
	} else {
		printf("Focused '%s' successfully.\n", dir);
	}
	free(result);

	if (g_config.haptic && g_haptic)
		haptic_actuate(g_haptic, 3);
}
```

Replace `reset_gesture_state` (lines 48-52) with:

```c
static void reset_gesture_state(gesture_ctx* ctx)
{
	ctx->state = GS_IDLE;
	ctx->last_fire_dir = 0;
	ctx->axis = AXIS_NONE;
}
```

- [ ] **Step 4: Route fire_gesture by axis**

Replace `fire_gesture` (lines 54-65) with:

```c
static void fire_gesture(gesture_ctx* ctx, int direction)
{
	if (direction == ctx->last_fire_dir)
		return;

	ctx->last_fire_dir = direction;
	ctx->state = GS_COMMITTED;

	int axis = ctx->axis;
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		if (axis == AXIS_VERTICAL)
			switch_focus(direction > 0 ? g_config.swipe_up : g_config.swipe_down);
		else
			switch_workspace(direction > 0 ? g_config.swipe_right : g_config.swipe_left);
	});
}
```

- [ ] **Step 5: Add y-velocity to calculate_touch_averages**

Replace `calculate_touch_averages` (lines 67-93) with (note the renamed `avg_vel_x` and new `avg_vel_y` out-params):

```c
static void calculate_touch_averages(touch* touches, int count,
	float* avg_x, float* avg_y, float* avg_vel_x, float* avg_vel_y,
	float* min_x, float* max_x, float* min_y, float* max_y)
{
	*avg_x = *avg_y = *avg_vel_x = *avg_vel_y = 0;
	*min_x = *min_y = 1;
	*max_x = *max_y = 0;

	for (int i = 0; i < count; ++i) {
		*avg_x += touches[i].x;
		*avg_y += touches[i].y;
		*avg_vel_x += touches[i].velocity;
		*avg_vel_y += touches[i].velocity_y;

		if (touches[i].x < *min_x)
			*min_x = touches[i].x;
		if (touches[i].x > *max_x)
			*max_x = touches[i].x;
		if (touches[i].y < *min_y)
			*min_y = touches[i].y;
		if (touches[i].y > *max_y)
			*max_y = touches[i].y;
	}

	*avg_x /= count;
	*avg_y /= count;
	*avg_vel_x /= count;
	*avg_vel_y /= count;
}
```

- [ ] **Step 6: Make handle_committed_state axis-aware**

Replace `handle_committed_state` (lines 95-127) with:

```c
static bool handle_committed_state(gesture_ctx* ctx, touch* touches, int count)
{
	bool all_ended = true;
	for (int i = 0; i < count; ++i) {
		if (touches[i].phase != END_PHASE) {
			all_ended = false;
			break;
		}
	}

	if (!count || all_ended) {
		reset_gesture_state(ctx);
		return true;
	}

	float avg_x, avg_y, avg_vel_x, avg_vel_y, min_x, max_x, min_y, max_y;
	calculate_touch_averages(touches, count, &avg_x, &avg_y, &avg_vel_x, &avg_vel_y,
		&min_x, &max_x, &min_y, &max_y);

	float primary_d = (ctx->axis == AXIS_VERTICAL) ? (avg_y - ctx->start_y)
												   : (avg_x - ctx->start_x);
	if ((primary_d * ctx->last_fire_dir) < 0 && fabsf(primary_d) >= g_config.min_travel) {
		ctx->state = GS_ARMED;
		ctx->start_x = avg_x;
		ctx->start_y = avg_y;
		ctx->peak_velx = avg_vel_x;
		ctx->peak_vely = avg_vel_y;
		ctx->dir = (primary_d >= 0) ? 1 : -1;

		for (int i = 0; i < count; ++i) {
			ctx->base_x[i] = touches[i].x;
			ctx->base_y[i] = touches[i].y;
		}
	}

	return true;
}
```

- [ ] **Step 7: Classify axis in handle_idle_state**

Replace `handle_idle_state` (lines 129-149) with:

```c
static void handle_idle_state(gesture_ctx* ctx, touch* touches, int count,
	float avg_x, float avg_y, float avg_vel_x, float avg_vel_y)
{
	bool fast = fabsf(avg_vel_x) >= g_config.velocity_pct * FAST_VEL_FACTOR ||
		fabsf(avg_vel_y) >= g_config.velocity_pct * FAST_VEL_FACTOR;
	float need = fast ? g_config.min_travel_fast : g_config.min_travel;

	bool moved = true;
	for (int i = 0; i < count && moved; ++i) {
		float mdx = fabsf(touches[i].x - ctx->base_x[i]);
		float mdy = fabsf(touches[i].y - ctx->base_y[i]);
		moved &= (mdx >= need || mdy >= need);
	}
	if (!moved)
		return;

	float dx = avg_x - ctx->start_x;
	float dy = avg_y - ctx->start_y;

	gesture_axis axis = decide_axis(dx, dy, fast, ACTIVATE_PCT);
	if (axis == AXIS_NONE)
		return;

	ctx->state = GS_ARMED;
	ctx->axis = axis;
	ctx->start_x = avg_x;
	ctx->start_y = avg_y;
	ctx->peak_velx = avg_vel_x;
	ctx->peak_vely = avg_vel_y;
	ctx->dir = (axis == AXIS_VERTICAL)
		? ((avg_vel_y >= 0) ? 1 : -1)
		: ((avg_vel_x >= 0) ? 1 : -1);
}
```

- [ ] **Step 8: Track the committed axis in handle_armed_state**

Replace `handle_armed_state` (lines 151-187) with:

```c
static void handle_armed_state(gesture_ctx* ctx, touch* touches, int count,
	float avg_x, float avg_y, float avg_vel_x, float avg_vel_y)
{
	float dx = avg_x - ctx->start_x;
	float dy = avg_y - ctx->start_y;

	bool vertical = (ctx->axis == AXIS_VERTICAL);
	float primary_d = vertical ? dy : dx;
	float cross_d = vertical ? dx : dy;
	float primary_vel = vertical ? avg_vel_y : avg_vel_x;
	float peak_vel = vertical ? ctx->peak_vely : ctx->peak_velx;

	// If the cross axis overtakes the committed axis, abandon the gesture.
	if (fabsf(cross_d) > fabsf(primary_d)) {
		reset_gesture_state(ctx);
		return;
	}

	bool fast = fabsf(primary_vel) >= g_config.velocity_pct * FAST_VEL_FACTOR;
	float stepReq = fast ? g_config.min_step_fast : g_config.min_step;

	int mismatch_count = 0;
	for (int i = 0; i < count; ++i) {
		float step = vertical ? (touches[i].y - ctx->prev_y[i])
							  : (touches[i].x - ctx->prev_x[i]);
		if (fabsf(step) < stepReq || (step * primary_d) < 0) {
			mismatch_count++;
			if (mismatch_count > g_config.swipe_tolerance) {
				reset_gesture_state(ctx);
				return;
			}
		}
	}

	if (fabsf(primary_vel) > fabsf(peak_vel)) {
		if (vertical)
			ctx->peak_vely = primary_vel;
		else
			ctx->peak_velx = primary_vel;
		ctx->dir = (primary_vel >= 0) ? 1 : -1;
	}

	if (fabsf(primary_vel) >= g_config.velocity_pct) {
		fire_gesture(ctx, primary_vel > 0 ? 1 : -1);
	} else if (fabsf(primary_d) >= g_config.distance_pct && fabsf(primary_vel) <= g_config.velocity_pct * g_config.settle_factor) {
		fire_gesture(ctx, primary_d > 0 ? 1 : -1);
	}
}
```

- [ ] **Step 9: Update gestureCallback for both axes**

Replace `gestureCallback` (lines 189-228) with:

```c
static void gestureCallback(touch* touches, int count)
{
	pthread_mutex_lock(&g_gesture_mutex);

	gesture_ctx* ctx = &g_gesture_ctx;

	if (ctx->state == GS_COMMITTED) {
		if (handle_committed_state(ctx, touches, count))
			goto unlock;
	}

	if (count != g_config.fingers) {
		if (ctx->state == GS_ARMED)
			ctx->state = GS_IDLE;

		for (int i = 0; i < count; ++i) {
			ctx->prev_x[i] = ctx->base_x[i] = touches[i].x;
			ctx->prev_y[i] = ctx->base_y[i] = touches[i].y;
		}

		goto unlock;
	}

	float avg_x, avg_y, avg_vel_x, avg_vel_y, min_x, max_x, min_y, max_y;
	calculate_touch_averages(touches, count, &avg_x, &avg_y, &avg_vel_x, &avg_vel_y,
		&min_x, &max_x, &min_y, &max_y);

	if (ctx->state == GS_IDLE) {
		handle_idle_state(ctx, touches, count, avg_x, avg_y, avg_vel_x, avg_vel_y);
	} else if (ctx->state == GS_ARMED) {
		handle_armed_state(ctx, touches, count, avg_x, avg_y, avg_vel_x, avg_vel_y);
	}

	for (int i = 0; i < count; ++i) {
		ctx->prev_x[i] = touches[i].x;
		ctx->prev_y[i] = touches[i].y;
		if (ctx->state == GS_IDLE) {
			ctx->base_x[i] = touches[i].x;
			ctx->base_y[i] = touches[i].y;
		}
	}

unlock:
	pthread_mutex_unlock(&g_gesture_mutex);
}
```

- [ ] **Step 10: Verify build + unit test**

Run: `make test && make`
Expected: PASS — `decide_axis` test passes; `swipe` builds with no new warnings.

- [ ] **Step 11: Commit**

```bash
git add src/event_tap.h src/main.m
git commit -m "feat: axis-aware swipe detection routing vertical to focus up/down"
```

---

### Task 6: Manual verification and docs

Confirm real trackpad behavior and document the new capability.

**Files:**
- Modify: `README.md` (features list)
- Modify: `config.md` (document `swipe_up`/`swipe_down` behavior — no new user-facing keys, but note vertical support)

- [ ] **Step 1: Build, sign, and restart the service**

Run: `make && make restart`
Expected: builds clean; launch agent reloads without error. (If not installed yet, run `make install` once.)

- [ ] **Step 2: Verify horizontal is unchanged**

With `fingers: 4` in `~/.config/aerospace-swipe/config.json`, perform a 4-finger **horizontal** swipe.
Expected: workspace switches (prev/next) exactly as before. Check `Console.app` / stderr for `Switched workspace successfully`.

- [ ] **Step 3: Verify vertical focus**

Perform a 4-finger **vertical** swipe up, then down.
Expected: focus moves up then down; log shows `Focused 'up' successfully.` / `Focused 'down' successfully.`. Confirm it matches your `alt-k`/`alt-j` bindings.

> If a vertical swipe hangs or errors on the socket, the `focus` command may need an explicit stdin flag like `workspace` does. Fix: in `aerospace.c` `execute_aerospace_command`, extend the stdin-flag guard (lines 291-294) to also emit `--no-stdin` for `focus`. Re-test.

- [ ] **Step 4: Verify diagonal rejection**

Perform slow diagonal 4-finger swipes.
Expected: the dominant axis wins; no double-fire (workspace AND focus) from one gesture.

- [ ] **Step 5: Update README**

In `README.md`, under `## features`, add a bullet:

```markdown
- vertical swipe support: a vertical x-fingered swipe changes window focus (aerospace `focus up`/`focus down`) while horizontal swipes still switch workspaces
```

- [ ] **Step 6: Update config.md**

In `config.md`, add a short section after `natural_swipe`:

```markdown
### vertical swipes

a vertical x-fingered swipe (same `fingers` count) sends aerospace `focus up` / `focus down` instead of switching workspaces. `natural_swipe` inverts the vertical mapping the same way it inverts horizontal: with `natural_swipe = true`, swipe up → `focus up`; with `natural_swipe = false`, swipe up → `focus down`.
```

- [ ] **Step 7: Commit and push**

```bash
git add README.md config.md
git commit -m "docs: document vertical swipe focus support"
git push
```

---

## Self-Review

**Spec coverage:**
- Axis classification → Task 1 (`decide_axis`) + Task 5 (`handle_idle_state`).
- Vertical velocity capture → Task 3.
- `gesture_ctx` y fields → Task 5 Step 1.
- `focus` over socket → Task 2 (`aerospace_focus`) + Task 5 (`switch_focus`).
- Config `swipe_up`/`swipe_down` + `natural_swipe` → Task 4.
- Horizontal unchanged → verified in Task 6 Step 2; horizontal branch preserved in `fire_gesture`/`handle_armed_state`.
- Diagonal / cross-axis handling → Task 5 Steps 7-8; verified Task 6 Step 4.
- Testing → Task 1 unit test; Task 6 manual.

**Placeholder scan:** none — all steps contain concrete code or exact commands. The Task 6 Step 3 note is a conditional contingency with an exact fix location, not a required TODO.

**Type consistency:** `decide_axis(float,float,bool,float)` and `AXIS_*` used consistently (Tasks 1, 5). `aerospace_focus(aerospace*, const char*)` declared (Task 2) and called (Task 5 Step 3). `calculate_touch_averages` new signature (`avg_vel_x`, `avg_vel_y`) used at all three call sites (Task 5 Steps 5, 6, 9). `touch.velocity` kept as x; `touch.velocity_y` added (Task 3) and read (Task 5 Step 5). `gesture_ctx.axis/peak_vely/prev_y/base_y` defined (Task 5 Step 1) and used throughout Task 5.
