/* Authored scalar fixtures, never events copied from either game's data.
 * This deliberately drives the observer, not gameplay or a natural boss route.
 */
#include "modern_progress.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
static void require(int condition, const char *description)
{
	++checks;
	if (!condition)
	{
		fprintf(stderr, "Progress fixture FAIL: %s\n", description);
		exit(EXIT_FAILURE);
	}
}
static void sample(ModernProgressState *state, unsigned position, bool paused)
{
	int before = state->percent;
	modern_progress_observer_tick(state, position, paused);
	require((state->percent >= 0 && state->percent <= 100) || (!state->supported && state->percent == -1), "bounds");
	require(state->percent >= before, "monotonic within attempt");
	require(state->percent != 100 || state->completed, "100 requires successful completion");
	if (paused && !state->completed)
		require(state->percent == before, "wait freezes traversal even with moving location");
}
int main(void)
{
	ModernProgressState s;
	modern_progress_observer_begin(&s, 1000);
	require(s.percent == 0 && !s.completed, "new attempt starts at zero");
	sample(&s, 100, false);
	sample(&s, 500, false);
	require(s.percent > 0 && s.percent < 100, "real traversal produces partial progress");
	int held = s.percent;
	/* A synthetic boss fight: moving route samples during active fight must
	 * not credit distance. This does not simulate boss spawn/recognition. */
	for (unsigned i = 0; i < 8; ++i) sample(&s, 500 + i * 100, true);
	require(s.percent == held && s.paused_ticks >= 8, "boss wait fixture executed");
	sample(&s, 650, false);
	require(!s.completed, "intermediate boss release does not imply victory");
	sample(&s, 10000, false);
	require(s.percent < 100, "overshooting endpoint cannot imply victory");
	modern_progress_observer_end(&s, true);
	require(s.percent == 100 && s.completed, "certified main-objective success fills gauge");
	sample(&s, 1, true);
	modern_progress_observer_end(&s, false);
	require(s.percent == 100, "success latch persists through later exit/presentation");
	puts("Progress fixture PASS: traversal boss-wait intermediate-release success-latch");

	modern_progress_observer_begin(&s, 1000);
	sample(&s, 100, false);
	sample(&s, 800, true);
	sample(&s, 900, false);
	require(s.percent == 19, "boss release permanently discards distance traversed during fight");
	puts("Progress fixture PASS: paused-clock-distance-not-recredited-on-release");

	modern_progress_observer_begin(&s, 1000);
	require(s.percent == 0 && s.jumps == 0 && !s.completed, "restart clears prior success and counters");
	sample(&s, 250, false);
	held = s.percent;
	modern_progress_observer_jump(&s, 250, 9000, false);
	require(s.percent == held, "far jump gives no address credit");
	sample(&s, 9010, true);
	modern_progress_observer_jump(&s, 9010, 251, true);
	require(s.percent == held && s.jumps > 0 && s.returns > 0, "return preserves fraction and records coverage");
	sample(&s, 252, false);
	modern_progress_observer_jump(&s, 252, 50, false);
	held = s.percent;
	for (unsigned i = 0; i < 8; ++i)
	{
		sample(&s, 100, true);
		modern_progress_observer_jump(&s, 100, 50, false);
		require(s.percent == held, "repeated wait loop never accumulates credit");
	}
	require(s.rewinds > 0, "rewind fixture actually executed");
	puts("Progress fixture PASS: distant-jump return rewind repeated-loop");

	modern_progress_observer_begin(&s, 1000);
	sample(&s, 250, false);
	held = s.percent;
	modern_progress_observer_jump(&s, 250, 600, false);
	require(s.percent == held && s.supported, "conditional skip keeps only remaining route budget");
	sample(&s, 700, false);
	require(s.percent > held && s.percent < 100, "conditional continuation advances without address credit");
	modern_progress_observer_begin(&s, 1000);
	sample(&s, 300, false);
	held = s.percent;
	for (unsigned i = 0; i < 8; ++i)
	{
		modern_progress_observer_jump(&s, 300, 50, false);
		sample(&s, 100, false);
		sample(&s, 250, false);
		require(s.percent == held && s.supported && s.loop_wait, "supported rewind freezes below prior frontier");
	}
	sample(&s, 500, false);
	require(s.percent > held && !s.loop_wait, "leaving rewind frontier resumes traversal");
	held = s.percent;
	for (unsigned i = 0; i < 4; ++i) sample(&s, 500, false);
	require(s.percent == held, "scroll stall and redraw add no traversal");
	/* Respawn is intentionally not an end call; retained state distinguishes
	 * it from a definitive death. An enemy-clear wait is not victory yet. */
	sample(&s, 600, false);
	require(s.percent >= held && !s.blocked && !s.completed, "respawn preserves live attempt");
	held = s.percent;
	for (unsigned i = 0; i < 4; ++i) sample(&s, 900, true);
	require(s.percent == held && !s.completed, "pending enemy-clear end waits for actual completion");
	modern_progress_observer_end(&s, true);
	require(s.percent == 100, "enemy-clear certified main-objective success fills gauge");
	puts("Progress fixture PASS: conditional-continuation supported-loop-resume scroll-stall respawn enemy-clear-wait");

	/* All negative runtime causes resolve to the observer's false outcome.
	 * This verifies the common latch, not completeness of runtime hooks. */
	const char *causes[] = { "death", "abandon", "forced-skip", "demo-eof", "demo-input", "timeout", "harness-cap" };
	for (unsigned i = 0; i < sizeof causes / sizeof causes[0]; ++i)
	{
		modern_progress_observer_begin(&s, 1000);
		sample(&s, 250, false);
		held = s.percent;
		modern_progress_observer_end(&s, false);
		modern_progress_observer_end(&s, true);
		require(s.percent == held && !s.completed && s.blocked, "negative cause blocks a later success signal");
		printf("Progress fixture PASS: negative-outcome=%s (synthetic common latch)\n", causes[i]);
	}
	/* A living early exit is not certified completion. The runtime supplies
	 * finish(false) for that outcome; reaching another marker cannot override it. */
	modern_progress_observer_begin(&s, 1000);
	sample(&s, 250, false);
	held = s.percent;
	modern_progress_observer_finish(&s, false);
	sample(&s, 900, false);
	require(s.percent == held && s.percent < 100 && !s.completed && s.ended && !s.blocked, "premature natural exit preserves partial value");
	modern_progress_observer_finish(&s, true);
	require(s.percent == held && !s.completed && s.ended && !s.blocked, "early finish cannot later promote via principal finish");
	modern_progress_observer_end(&s, true);
	require(s.percent == held && !s.completed && s.ended, "closed early attempt ignores later completion callback");
	modern_progress_observer_begin(&s, 0);
	sample(&s, 65535, false);
	modern_progress_observer_finish(&s, false);
	require(s.percent == -1 && !s.completed && s.ended && !s.blocked, "uncertified unknown-route exit cannot fill gauge");
	puts("Progress fixture PASS: premature-natural-exit unknown-route-uncertified-end");
	modern_progress_observer_begin(&s, 0);
	sample(&s, 65535, false);
	require(s.percent == -1 && !s.supported, "unknown endpoint explicitly unavailable");
	modern_progress_observer_finish(&s, true);
	require(s.percent == 100 && s.completed, "external certified objective proof can complete unknown traversal");
	printf("Progress fixture PASS: unsupported reset checks=%u\n", checks);
	return EXIT_SUCCESS;
}
