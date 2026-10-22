#include <stdlib.h>

#include "core/daily.h"
#include "test.h"

static int load_bank(WordList *list)
{
    return wordlist_load(list, SIXDLE_WORDS) == 0
        && wordlist_load_frequencies(list, SIXDLE_FREQ) == 0
        && wordlist_load_never_secret(list, SIXDLE_NEVER_SECRET) == 0;
}

static void puzzle_numbers_follow_the_calendar(void)
{
    CHECK_INT(daily_number(2026, 1, 1), 1);
    CHECK_INT(daily_number(2026, 1, 2), 2);
    CHECK_INT(daily_number(2025, 12, 31), 0);
    CHECK_INT(daily_number(2026, 3, 1), 60); // not a leap year
    CHECK_INT(daily_number(2026, 10, 22), 295);
    CHECK_INT(daily_number(2027, 1, 1), 366);
    CHECK_INT(daily_number(2028, 3, 1) - daily_number(2028, 2, 28), 2);
    CHECK(daily_today() >= 295); // written on puzzle #295
    long left = daily_seconds_left();
    CHECK(left > 0 && left <= 25 * 3600); // DST days can be 25 hours
}

static void same_word_for_everyone(void)
{
    WordList list;
    REQUIRE(load_bank(&list));
    // Frozen: if this changes, everyone's daily words change. Only update
    // it on purpose (e.g. when editing the word lists).
    CHECK_STR(list.items[daily_word(&list, 1)], "hurdle");
    CHECK_INT(daily_word(&list, 42), daily_word(&list, 42));
    CHECK(daily_word(&list, 42) != daily_word(&list, 43));
    wordlist_free(&list);
}

static void every_familiar_word_once_per_cycle(void)
{
    WordList list;
    REQUIRE(load_bank(&list));
    size_t n = daily_pool_size(&list);
    CHECK(n > 3000 && n < 4000);
    unsigned char *seen = calloc(list.count, 1);
    int repeats = 0;
    int bad = 0;
    for (long day = 1; day <= (long)n; day++)
    {
        long w = daily_word(&list, day);
        REQUIRE(w >= 0);
        repeats += seen[w];
        seen[w] = 1;
        bad +=
            list.zipf[w] < DAILY_ZIPF || !word_can_be_secret(&list, (size_t)w);
    }
    CHECK_INT(repeats, 0); // a whole cycle without repeating a word
    CHECK_INT(bad, 0); // only familiar words, never a blocked one
    // The cycle then starts over, and dates before #1 wrap around.
    CHECK_INT(daily_word(&list, (long)n + 1), daily_word(&list, 1));
    CHECK_INT(daily_word(&list, 0), daily_word(&list, (long)n));
    free(seen);

    WordList bare;
    REQUIRE(wordlist_load(&bare, FIXTURE("small.txt")) == 0);
    CHECK_INT(daily_word(&bare, 1), -1); // no frequencies: no daily
    wordlist_free(&bare);
    wordlist_free(&list);
}

static void share_text(void)
{
    const char *words[] = { "planet", "tories", "planer" };
    WordList list;
    REQUIRE(wordlist_from_array(&list, words, 3) == 0);
    Game game;
    game_start(&game, &list, "planet");
    game_guess(&game, "tories");
    game_guess(&game, "planer");
    game_guess(&game, "planet");
    char buf[512];
    size_t n = daily_share(&game, 295, buf, sizeof(buf));
    CHECK_STR(buf,
              "6dle #295 3/7\n"
              "🟨⬛⬛⬛🟩⬛\n"
              "🟩🟩🟩🟩🟩⬛\n"
              "🟩🟩🟩🟩🟩🟩\n");
    CHECK_INT(n, strlen(buf));

    game_start(&game, &list, "planet");
    game_guess(&game, "tories");
    game.status = GAME_LOST; // as if out of guesses
    daily_share(&game, 7, buf, sizeof(buf));
    CHECK(strncmp(buf, "6dle #7 X/7\n", 12) == 0);
    // Truncation never overflows and keeps the string terminated.
    char small[10];
    daily_share(&game, 7, small, sizeof(small));
    CHECK_INT(strlen(small), sizeof(small) - 1);
    CHECK_INT(daily_share(&game, 7, small, 0), 0);
    wordlist_free(&list);
}

void daily_tests(void)
{
    RUN(puzzle_numbers_follow_the_calendar);
    RUN(same_word_for_everyone);
    RUN(every_familiar_word_once_per_cycle);
    RUN(share_text);
}
