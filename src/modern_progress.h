/* Observation-only level traversal; no gameplay state or RNG ownership. */
#ifndef MODERN_PROGRESS_H
#define MODERN_PROGRESS_H

#include <stdbool.h>

/* Stable draw API: querying never advances or changes the observer. */
int modern_progress_percent(void);
bool modern_progress_paused(void);

/* Scalar-only model exposed for synthetic tests. Endpoint is a route milestone,
 * never the final event/sentinel; zero means traversal is unavailable. */
typedef struct ModernProgressState
{
	unsigned endpoint, frontier, segment_origin, segment_distance;
	int percent, segment_percent;
	bool supported, paused, blocked, completed, ended;
	unsigned jumps, returns, rewinds, paused_ticks, advances, completions;
	unsigned attempt, ticks, events, early_ends;
	bool boss_wait, loop_wait;
} ModernProgressState;

void modern_progress_observer_begin(ModernProgressState *state, unsigned endpoint);
void modern_progress_observer_tick(ModernProgressState *state, unsigned position, bool paused);
void modern_progress_observer_jump(ModernProgressState *state, unsigned before, unsigned after, bool returning);
/* success here means an explicitly proved principal objective, not merely exit. */
void modern_progress_observer_end(ModernProgressState *state, bool success);
void modern_progress_observer_finish(ModernProgressState *state, bool principal);

/* Runtime hooks accept decisions already made by the game. */
void modern_progress_set_observer_enabled(bool enabled);
void modern_progress_begin(void);
void modern_progress_tick(void);
void modern_progress_jump(unsigned before, unsigned after, bool returning);
void modern_progress_reposition(unsigned before, unsigned after);
void modern_progress_end(bool natural);
void modern_progress_cancel(void);
void modern_progress_timer_expired(void);
void modern_progress_resolve_end(void);
void modern_progress_event(int action, unsigned index);
const ModernProgressState *modern_progress_snapshot(void);

#endif
