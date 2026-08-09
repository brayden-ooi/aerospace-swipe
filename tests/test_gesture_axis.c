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
