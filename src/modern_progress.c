/* OpenTyrian GPL-2.0-or-later. Presentation-only traversal observer.
 * Route metadata is derived in memory, never copied into per-map event tables.
 */
#include "modern_progress.h"

#include <stdint.h>
#include <string.h>

void modern_progress_observer_begin(ModernProgressState *s, unsigned endpoint)
{
	memset(s, 0, sizeof *s);
	s->endpoint = endpoint;
	s->supported = endpoint != 0;
	s->percent = s->supported ? 0 : -1;
}

void modern_progress_observer_tick(ModernProgressState *s, unsigned position, bool paused)
{
	++s->ticks;
	s->loop_wait = position < s->frontier;
	s->paused = paused || s->loop_wait || !s->supported || s->blocked || s->completed || s->ended;
	if (s->paused)
	{
		++s->paused_ticks;
		/* Waiting can advance the script clock (forceEvents). Discard that
		 * motion now so releasing a boss/wait cannot credit it retrospectively. */
		if (position > s->frontier)
			s->frontier = position;
		return;
	}
	if (position <= s->frontier || s->endpoint <= s->segment_origin)
		return;
	unsigned delta = position - s->frontier;
	s->frontier = position;
	unsigned length = s->endpoint - s->segment_origin;
	if (delta > length - s->segment_distance)
		delta = length - s->segment_distance;
	s->segment_distance += delta;
	int percent = s->segment_percent + (int)((uint64_t)(99u - (unsigned)s->segment_percent) * s->segment_distance / length);
	if (percent > s->percent)
	{
		s->percent = percent;
		++s->advances;
	}
}

void modern_progress_observer_jump(ModernProgressState *s, unsigned before, unsigned after, bool returning)
{
	++s->jumps;
	if (returning)
	{
		++s->returns;
		/* A return without a certified call route cannot credit remote addresses.
		 * Keep the last estimate, and let positive completion remain possible. */
		s->supported = false;
	}
	if (after <= before)
	{
		++s->rewinds;
		if (before > s->frontier)
			s->frontier = before;
		s->loop_wait = true;
	}
	else if (s->supported)
	{
		/* Skip addresses do not count as traversal. Spend only the remaining
		 * visual budget on the continuation chosen by the actual game. */
		s->segment_origin = after;
		s->frontier = after;
		s->segment_percent = s->percent;
		s->segment_distance = 0;
		if (after >= s->endpoint)
			s->supported = false;
	}
	s->paused = true;
}

void modern_progress_observer_end(ModernProgressState *s, bool success)
{
	if (s->completed || s->ended)
		return;
	if (!success)
		s->blocked = true;
	else if (!s->blocked)
	{
		s->percent = 100;
		s->completed = true;
		++s->completions;
	}
	s->paused = true;
}

void modern_progress_observer_finish(ModernProgressState *s, bool principal)
{
	if (principal)
		modern_progress_observer_end(s, true);
	else if (!s->completed && !s->ended)
	{
		++s->early_ends;
		s->ended = true;
		s->paused = true;
	}
}

#if !defined(MODERN_PROGRESS_STANDALONE) && !defined(MODERN_PROGRESS_FIXTURE)
#include "backgrnd.h"
#include "episodes.h"
#include "game_rules.h"
#include "modern.h"
#include "player.h"
#include "tyrian2.h"
#include "varz.h"

extern struct JE_EventRecType eventRec[EVENT_MAXIMUM];

static ModernProgressState progress = { .percent = -1 };
static unsigned attempt;
static bool observer_enabled = true;
static bool natural_pending;
static unsigned goal_event;
static bool goal_seen, pending_principal;

int modern_progress_percent(void) { return progress.percent; }
bool modern_progress_paused(void) { return progress.paused; }
const ModernProgressState *modern_progress_snapshot(void) { return &progress; }

void modern_progress_set_observer_enabled(bool enabled)
{
	observer_enabled = enabled;
	if (!enabled)
		modern_progress_observer_begin(&progress, 0);
}

static bool observing(void)
{
	return observer_enabled && (presentation == PRESENTATION_MODERN);
}

void modern_progress_begin(void)
{
	unsigned endpoint = 0;
	goal_event = 0;
	bool return_route = false;
	if (observing())
	{
		for (unsigned i = 0; i < maxEvent; ++i)
		{
			const struct JE_EventRecType *e = &eventRec[i];
			int action = gameEventCase(e->eventtype);
			if ((action == 11 || action == 36) && e->eventtime && e->eventtime >= endpoint)
			{
				/* Furthest semantic exit milestone is a conservative principal
				 * route estimate. Earlier exits may finish below 100%. */
				endpoint = e->eventtime;
				goal_event = i + 1;
			}
			if (action == 76 || ((action == 54 || action == 70 || action == 71) && (JE_word)e->eventdat == 65535))
				return_route = true;
		}
	}
	/* Neither final serialized timestamp nor a remote subroutine is a route
	 * endpoint. End-marker-free/return routes have no defensible denominator. */
	modern_progress_observer_begin(&progress, return_route ? 0 : endpoint);
	progress.attempt = observing() ? ++attempt : 0;
	natural_pending = goal_seen = pending_principal = false;
	if (return_route)
		goal_event = 0;
}

void modern_progress_event(int action, unsigned index)
{
	if (observing())
	{
		++progress.events;
		if ((action == 11 || action == 36) && goal_event != 0 && index == goal_event)
			goal_seen = true;
	}
}

void modern_progress_resolve_end(void)
{
	if (!observing())
		return;
	if (playerEndLevel)
		modern_progress_observer_end(&progress, false);
	if (natural_pending)
	{
		/* Current living state, not allPlayersGone cached at tick start. One
		 * survivor is enough in 2P; a dead bonus level is never a victory. */
		if (all_players_dead())
			modern_progress_observer_end(&progress, false);
		else
			modern_progress_observer_finish(&progress, pending_principal);
		natural_pending = false;
	}
}

void modern_progress_tick(void)
{
	if (!observing())
		return;
	modern_progress_resolve_end();
	bool boss = false;
	for (unsigned b = 0; b < 2; ++b)
	{
		if (!boss_bar[b].link_num)
			continue;
		for (unsigned e = 0; e < 100; ++e)
			if (enemyAvail[e] != 1 && enemy[e].armorleft > 0 && enemy[e].linknum == boss_bar[b].link_num)
				boss = true;
	}
	progress.boss_wait = boss;
	modern_progress_observer_tick(&progress, curLoc,
		boss || all_players_dead() || returnActive || (backMove == 0 && !forceEvents) || readyToEndLevel || endLevel || reallyEndLevel);
}

void modern_progress_jump(unsigned before, unsigned after, bool returning)
{
	if (observing())
		modern_progress_observer_jump(&progress, before, after, returning);
}

void modern_progress_reposition(unsigned before, unsigned after)
{
	if (observing() && before != after)
	{
		modern_progress_observer_jump(&progress, before, after, false);
		/* Cases 38/75 can change the event cursor with different lookup rules.
		 * Freeze an existing estimate rather than assume an equivalent route. */
		progress.supported = false;
	}
}

void modern_progress_end(bool natural)
{
	if (observing())
	{
		if (natural)
		{
			natural_pending = true;
			pending_principal = goal_seen && goal_event != 0 && curLoc >= progress.endpoint;
		}
		else
			modern_progress_observer_end(&progress, false);
	}
}

void modern_progress_cancel(void) { modern_progress_end(false); }

void modern_progress_timer_expired(void)
{
	/* Authorial outcome policy, not copied event data: scratch runtime probes
	 * verify the common E4/L5 survival route in both variants. Other timer
	 * exits (including battle limits) are conservatively uncertified. */
	static const struct { unsigned episode, level; } survival[] = { { 4, 5 } };
	bool certified = false;
	for (unsigned i = 0; i < sizeof survival / sizeof survival[0]; ++i)
		if (episodeNum == survival[i].episode && lvlFileNum == survival[i].level)
			certified = true;
	if (timedBattleMode || !certified)
		modern_progress_cancel();
}
#endif
