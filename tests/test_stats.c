#include <stdlib.h>
#include <unistd.h>

#include "core/stats.h"
#include "test.h"

static const char *const WORDS[] = { "planet", "planer", "tories", "teeter",
                                     "muddly", "guzzle", "puzzle", "candle" };

// A finished game: won in `guesses` guesses, or lost if guesses == 0.
static Game finished(const WordList *list, int guesses)
{
    Game game;
    game_start(&game, list, "planet");
    int target = guesses ? guesses - 1 : MAX_GUESSES;
    for (size_t i = 0; game.nguesses < target && i < list->count; i++)
    {
        if (strcmp(list->items[i], "planet") != 0)
            game_guess(&game, list->items[i]);
    }
    if (guesses)
        game_guess(&game, "planet");
    return game;
}

static void free_play_streaks(void)
{
    WordList list;
    REQUIRE(wordlist_from_array(&list, WORDS, 8) == 0);
    Stats st;
    stats_init(&st);
    CHECK_INT(st.free_play.last_result, -1);
    CHECK_INT(stats_win_percent(&st.free_play), 0);

    Game won3 = finished(&list, 3);
    Game won1 = finished(&list, 1);
    Game lost = finished(&list, 0);
    REQUIRE(won3.status == GAME_WON && won3.nguesses == 3);
    REQUIRE(lost.status == GAME_LOST);
    stats_record_free(&st, &won3);
    stats_record_free(&st, &won1);
    CHECK_INT(st.free_play.streak, 2);
    stats_record_free(&st, &lost);
    CHECK_INT(st.free_play.streak, 0);
    CHECK_INT(st.free_play.best_streak, 2);
    stats_record_free(&st, &won3);
    CHECK_INT(st.free_play.played, 4);
    CHECK_INT(st.free_play.won, 3);
    CHECK_INT(st.free_play.dist[2], 2); // two wins in 3
    CHECK_INT(st.free_play.dist[0], 1);
    CHECK_INT(st.free_play.last_result, 3);
    CHECK_INT(stats_win_percent(&st.free_play), 75);
    CHECK_INT(st.daily.played, 0); // separate statistics

    Game playing;
    game_start(&playing, &list, "planet");
    stats_record_free(&st, &playing); // unfinished: ignored
    CHECK_INT(st.free_play.played, 4);
    wordlist_free(&list);
}

static void daily_streak_counts_days(void)
{
    WordList list;
    REQUIRE(wordlist_from_array(&list, WORDS, 8) == 0);
    Stats st;
    stats_init(&st);
    Game won = finished(&list, 4);
    Game lost = finished(&list, 0);

    stats_record_daily(&st, &won, 10);
    stats_record_daily(&st, &won, 11);
    stats_record_daily(&st, &won, 12);
    CHECK_INT(st.daily.streak, 3);
    CHECK_INT(stats_daily_streak(&st, 12), 3); // played today
    CHECK_INT(stats_daily_streak(&st, 13), 3); // today not played yet
    CHECK_INT(stats_daily_streak(&st, 14), 0); // missed a day

    stats_record_daily(&st, &won, 12); // same puzzle again: ignored
    CHECK_INT(st.daily.played, 3);
    stats_record_daily(&st, &won, 14); // after a gap: starts over
    CHECK_INT(st.daily.streak, 1);
    stats_record_daily(&st, &lost, 15);
    CHECK_INT(st.daily.streak, 0);
    CHECK_INT(st.daily.best_streak, 3);
    CHECK_INT(st.daily.played, 5);
    CHECK_INT(st.daily.won, 4);
    CHECK_INT(st.daily.dist[3], 4);
    CHECK_INT(st.daily.last_result, 0);
    // Recording a daily also keeps its board.
    CHECK_INT(st.progress_day, 15);
    CHECK_INT(st.nprogress, MAX_GUESSES);
    wordlist_free(&list);
}

static void save_and_load(void)
{
    char dir[] = "/tmp/6dle-test-stats-XXXXXX";
    REQUIRE(mkdtemp(dir) != NULL);
    char path[256];
    snprintf(path, sizeof(path), "%s/sub/dir/stats.txt", dir);

    WordList list;
    REQUIRE(wordlist_from_array(&list, WORDS, 8) == 0);
    Stats st;
    stats_init(&st);
    Game won = finished(&list, 2);
    stats_record_free(&st, &won);
    stats_record_daily(&st, &won, 295);
    Game partial;
    game_start(&partial, &list, "planet");
    game_guess(&partial, "tories");
    game_guess(&partial, "teeter");
    stats_set_progress(&st, &partial, 296);
    CHECK(stats_save(&st, path) == 0); // creates sub/dir

    Stats back;
    REQUIRE(stats_load(&back, path) == 0);
    CHECK(memcmp(&back.daily, &st.daily, sizeof(st.daily)) == 0);
    CHECK(memcmp(&back.free_play, &st.free_play, sizeof(st.free_play)) == 0);
    CHECK_INT(back.progress_day, 296);
    CHECK_INT(back.nprogress, 2);
    CHECK_STR(back.progress[1], "teeter");

    // Missing file: empty statistics, not an error.
    char missing[256];
    snprintf(missing, sizeof(missing), "%s/nope.txt", dir);
    CHECK(stats_load(&back, missing) == 0);
    CHECK_INT(back.daily.played, 0);

    // Garbage is ignored, valid lines are kept.
    FILE *f = fopen(path, "w");
    REQUIRE(f != NULL);
    fprintf(f,
            "daily.played 5\nnonsense\nfree.won abc\n"
            "free.last_result 99\nprogress 7 ab!cde tories\n"
            "daily.dist 1 2 3\n");
    fclose(f);
    REQUIRE(stats_load(&back, path) == 0);
    CHECK_INT(back.daily.played, 5);
    CHECK_INT(back.daily.dist[2], 3);
    CHECK_INT(back.free_play.won, 0);
    CHECK_INT(back.free_play.last_result, -1);
    CHECK_INT(back.nprogress, 0); // garbled progress dropped

    unlink(path);
    char sub[256];
    snprintf(sub, sizeof(sub), "%s/sub/dir", dir);
    rmdir(sub);
    snprintf(sub, sizeof(sub), "%s/sub", dir);
    rmdir(sub);
    rmdir(dir);
    wordlist_free(&list);
}

static void default_location(void)
{
    char *saved_custom = getenv("SIXDLE_STATS");
    char *saved_xdg = getenv("XDG_DATA_HOME");
    char custom_copy[512] = "";
    char xdg_copy[512] = "";
    if (saved_custom)
        snprintf(custom_copy, sizeof(custom_copy), "%s", saved_custom);
    if (saved_xdg)
        snprintf(xdg_copy, sizeof(xdg_copy), "%s", saved_xdg);

    char path[512];
    setenv("SIXDLE_STATS", "/tmp/custom.txt", 1);
    CHECK(stats_default_path(path, sizeof(path)) == 0);
    CHECK_STR(path, "/tmp/custom.txt");
    unsetenv("SIXDLE_STATS");
    setenv("XDG_DATA_HOME", "/data", 1);
    CHECK(stats_default_path(path, sizeof(path)) == 0);
    CHECK_STR(path, "/data/6dle/stats.txt");
    unsetenv("XDG_DATA_HOME");
    CHECK(stats_default_path(path, sizeof(path)) == 0);
    CHECK(strstr(path, "/.local/share/6dle/stats.txt") != NULL);
    CHECK(stats_default_path(path, 8) == -1); // too small

    if (saved_custom)
        setenv("SIXDLE_STATS", custom_copy, 1);
    if (saved_xdg)
        setenv("XDG_DATA_HOME", xdg_copy, 1);
}

void stats_tests(void)
{
    RUN(free_play_streaks);
    RUN(daily_streak_counts_days);
    RUN(save_and_load);
    RUN(default_location);
}
