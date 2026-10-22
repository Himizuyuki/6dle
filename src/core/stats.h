#ifndef STATS_H
#define STATS_H

#include <stddef.h>

#include "game.h"

// The player's statistics, kept separately for daily and free play.
typedef struct StatsBlock
{
    unsigned played;
    unsigned won;
    unsigned streak; // see stats_streak
    unsigned best_streak;
    unsigned dist[MAX_GUESSES]; // dist[i]: games won in i + 1 guesses
    int last_result; // guesses of the last game, 0 if lost, -1 if none
    long last_day; // daily only: puzzle number of the last daily played
} StatsBlock;

typedef struct Stats
{
    StatsBlock daily;
    StatsBlock free_play;
    // The daily game of puzzle progress_day (0: none), saved after every
    // guess so that quitting doesn't give a second try.
    long progress_day;
    int nprogress;
    char progress[MAX_GUESSES][WORD_LEN + 1];
} Stats;

void stats_init(Stats *stats);

// Read the statistics file. A missing file gives empty statistics and
// returns 0; unknown or malformed lines are ignored. Returns -1 only if the
// file exists but can't be read.
int stats_load(Stats *stats, const char *path);

// Write the file atomically (temporary file + rename), creating its
// directory if needed. Returns 0 on success.
int stats_save(const Stats *stats, const char *path);

// Where the file lives: $SIXDLE_STATS if set, else
// $XDG_DATA_HOME/6dle/stats.txt, else ~/.local/share/6dle/stats.txt.
// Returns 0 on success, -1 if no location could be determined.
int stats_default_path(char *buf, size_t size);

// Record a finished free-play game. The streak counts consecutive wins.
void stats_record_free(Stats *stats, const Game *game);

// Record a finished daily game of puzzle `day` (ignored if that puzzle was
// already recorded). The streak counts consecutive days won.
void stats_record_daily(Stats *stats, const Game *game, long day);

// Remember the guesses of the daily game of puzzle `day`.
void stats_set_progress(Stats *stats, const Game *game, long day);

// Current daily streak as of puzzle `today`: a missed day breaks it.
unsigned stats_daily_streak(const Stats *stats, long today);

// Percentage of games won (0 when none played).
unsigned stats_win_percent(const StatsBlock *block);

#endif // STATS_H
