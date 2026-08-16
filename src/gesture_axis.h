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
