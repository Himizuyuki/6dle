#include "stats.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * File format: plain text, one "key value..." per line, '#' for comments.
 *   daily.played 12        (same keys with "free." for free play)
 *   daily.won 11
 *   daily.streak 3
 *   daily.best_streak 7
 *   daily.last_result 4
 *   daily.last_day 294
 *   daily.dist 0 2 5 3 1 0 0
 *   progress 295 tories halted
 */

void stats_init(Stats *stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->daily.last_result = -1;
    stats->free_play.last_result = -1;
}

static void record(StatsBlock *b, const Game *game)
{
    b->played++;
    if (game->status == GAME_WON)
    {
        b->won++;
        b->dist[game->nguesses - 1]++;
        b->last_result = game->nguesses;
    }
    else
        b->last_result = 0;
}

void stats_record_free(Stats *stats, const Game *game)
{
    if (game->status == GAME_PLAYING)
        return;
    StatsBlock *b = &stats->free_play;
    record(b, game);
    b->streak = game->status == GAME_WON ? b->streak + 1 : 0;
    if (b->streak > b->best_streak)
        b->best_streak = b->streak;
}

void stats_record_daily(Stats *stats, const Game *game, long day)
{
    StatsBlock *b = &stats->daily;
    if (game->status == GAME_PLAYING || (b->played > 0 && day <= b->last_day))
        return;
    int continues = b->played > 0 && b->last_day == day - 1 && b->streak > 0;
    record(b, game);
    if (game->status == GAME_WON)
        b->streak = continues ? b->streak + 1 : 1;
    else
        b->streak = 0;
    if (b->streak > b->best_streak)
        b->best_streak = b->streak;
    b->last_day = day;
    stats_set_progress(stats, game, day);
}

void stats_set_progress(Stats *stats, const Game *game, long day)
{
    stats->progress_day = day;
    stats->nprogress = game->nguesses;
    memcpy(stats->progress, game->guesses, sizeof(stats->progress));
}

unsigned stats_daily_streak(const Stats *stats, long today)
{
    const StatsBlock *b = &stats->daily;
    if (b->played == 0 || today > b->last_day + 1)
        return 0;
    return b->streak;
}

unsigned stats_win_percent(const StatsBlock *block)
{
    if (block->played == 0)
        return 0;
    return (unsigned)(100.0 * block->won / block->played + 0.5);
}

/* ------------------------------------------------------------------ file */

static void parse_field(StatsBlock *b, const char *field, const char *value)
{
    if (strcmp(field, "played") == 0)
        sscanf(value, "%u", &b->played);
    else if (strcmp(field, "won") == 0)
        sscanf(value, "%u", &b->won);
    else if (strcmp(field, "streak") == 0)
        sscanf(value, "%u", &b->streak);
    else if (strcmp(field, "best_streak") == 0)
        sscanf(value, "%u", &b->best_streak);
    else if (strcmp(field, "last_result") == 0)
        sscanf(value, "%d", &b->last_result);
    else if (strcmp(field, "last_day") == 0)
        sscanf(value, "%ld", &b->last_day);
    else if (strcmp(field, "dist") == 0)
    {
        for (int i = 0; i < MAX_GUESSES; i++)
        {
            int used = 0;
            if (sscanf(value, "%u%n", &b->dist[i], &used) != 1)
                break;
            value += used;
        }
    }
    if (b->last_result < -1 || b->last_result > MAX_GUESSES)
        b->last_result = -1;
}

static void parse_progress(Stats *stats, const char *value)
{
    long day;
    int used = 0;
    if (sscanf(value, "%ld%n", &day, &used) != 1)
        return;
    value += used;
    int n = 0;
    char word[16];
    while (n < MAX_GUESSES && sscanf(value, "%15s%n", word, &used) == 1)
    {
        if (!word_is_valid(word))
            return; // garbled: ignore the whole line
        memcpy(stats->progress[n++], word, WORD_LEN + 1);
        value += used;
    }
    stats->progress_day = day;
    stats->nprogress = n;
}

int stats_load(Stats *stats, const char *path)
{
    stats_init(stats);
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return errno == ENOENT ? 0 : -1;
    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, f) != -1)
    {
        char key[32];
        int used = 0;
        if (line[0] == '#' || sscanf(line, "%31s%n", key, &used) != 1)
            continue;
        const char *value = line + used;
        if (strncmp(key, "daily.", 6) == 0)
            parse_field(&stats->daily, key + 6, value);
        else if (strncmp(key, "free.", 5) == 0)
            parse_field(&stats->free_play, key + 5, value);
        else if (strcmp(key, "progress") == 0)
            parse_progress(stats, value);
    }
    free(line);
    fclose(f);
    return 0;
}

// mkdir -p of the directory part of path.
static int make_parent_dirs(const char *path)
{
    char dir[4096];
    if (strlen(path) >= sizeof(dir))
        return -1;
    strcpy(dir, path);
    for (char *p = dir + 1; *p != '\0'; p++)
    {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(dir, 0755) != 0 && errno != EEXIST)
            return -1;
        *p = '/';
    }
    return 0;
}

static void write_block(FILE *f, const char *name, const StatsBlock *b)
{
    fprintf(f, "%s.played %u\n", name, b->played);
    fprintf(f, "%s.won %u\n", name, b->won);
    fprintf(f, "%s.streak %u\n", name, b->streak);
    fprintf(f, "%s.best_streak %u\n", name, b->best_streak);
    fprintf(f, "%s.last_result %d\n", name, b->last_result);
    fprintf(f, "%s.last_day %ld\n", name, b->last_day);
    fprintf(f, "%s.dist", name);
    for (int i = 0; i < MAX_GUESSES; i++)
        fprintf(f, " %u", b->dist[i]);
    fprintf(f, "\n");
}

int stats_save(const Stats *stats, const char *path)
{
    char tmp[4096];
    if (make_parent_dirs(path) != 0
        || snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp))
        return -1;
    FILE *f = fopen(tmp, "w");
    if (f == NULL)
        return -1;
    fprintf(f, "# 6dle statistics, written by the game\n");
    write_block(f, "daily", &stats->daily);
    write_block(f, "free", &stats->free_play);
    if (stats->nprogress > 0)
    {
        fprintf(f, "progress %ld", stats->progress_day);
        for (int i = 0; i < stats->nprogress; i++)
            fprintf(f, " %s", stats->progress[i]);
        fprintf(f, "\n");
    }
    int ok = !ferror(f);
    if (fclose(f) != 0)
        ok = 0;
    // Rename over the old file: a crash never leaves a half-written one.
    if (!ok || rename(tmp, path) != 0)
    {
        remove(tmp);
        return -1;
    }
    return 0;
}

int stats_default_path(char *buf, size_t size)
{
    const char *custom = getenv("SIXDLE_STATS");
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    int n;
    if (custom != NULL && custom[0] != '\0')
        n = snprintf(buf, size, "%s", custom);
    else if (xdg != NULL && xdg[0] == '/')
        n = snprintf(buf, size, "%s/6dle/stats.txt", xdg);
    else if (home != NULL && home[0] != '\0')
        n = snprintf(buf, size, "%s/.local/share/6dle/stats.txt", home);
    else
        return -1;
    return n > 0 && (size_t)n < size ? 0 : -1;
}
