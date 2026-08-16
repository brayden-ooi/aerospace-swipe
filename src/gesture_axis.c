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
