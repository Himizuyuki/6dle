#include "core/game.h"
#include "test.h"

static const char *const WORDS[] = { "planet", "planer", "tories", "teeter",
                                     "muddly", "guzzle", "puzzle", "candle" };
#define NWORDS (sizeof(WORDS) / sizeof(WORDS[0]))

static void guesses_are_validated(void)
{
    WordList list;
    REQUIRE(wordlist_from_array(&list, WORDS, NWORDS) == 0);
    Game game;
    game_start(&game, &list, "planet");
    CHECK_INT(game.status, GAME_PLAYING);
    CHECK_INT(game_guess(&game, "plane"), GUESS_BAD_LENGTH);
    CHECK_INT(game_guess(&game, "planets"), GUESS_BAD_LENGTH);
    CHECK_INT(game_guess(&game, "zzzzzz"), GUESS_NOT_A_WORD);
    CHECK_INT(game.nguesses, 0); // rejected guesses cost nothing
    CHECK_INT(game_guess(&game, "TORIES"), GUESS_OK);
    CHECK_STR(game.guesses[0], "tories");
    CHECK_INT(game_guess(&game, "tories"), GUESS_REPEATED);
    CHECK_INT(game.nguesses, 1);
    CHECK(guess_result_message(GUESS_NOT_A_WORD)[0] != '\0');
    wordlist_free(&list);
}

static void winning_ends_the_game(void)
{
    WordList list;
    REQUIRE(wordlist_from_array(&list, WORDS, NWORDS) == 0);
    Game game;
    game_start(&game, &list, "PLANET");
    CHECK_STR(game.secret, "planet");
    CHECK_INT(game_guess(&game, "tories"), GUESS_OK);
    CHECK_INT(game_guess(&game, "planet"), GUESS_OK);
    CHECK_INT(game.status, GAME_WON);
    CHECK_INT(game.nguesses, 2);
    CHECK_INT(game.patterns[1], PATTERN_WIN);
    CHECK_INT(game_guess(&game, "candle"), GUESS_GAME_OVER);
    CHECK_INT(game.nguesses, 2);
    wordlist_free(&list);
}

static void running_out_of_guesses_loses(void)
{
    WordList list;
    REQUIRE(wordlist_from_array(&list, WORDS, NWORDS) == 0);
    Game game;
    game_start(&game, &list, "planet");
    int played = 0;
    for (size_t i = 0; i < NWORDS && played < MAX_GUESSES; i++)
    {
        if (strcmp(list.items[i], "planet") == 0)
            continue;
        CHECK_INT(game_guess(&game, list.items[i]), GUESS_OK);
        played++;
        CHECK_INT(game.status,
                  played == MAX_GUESSES ? GAME_LOST : GAME_PLAYING);
    }
    CHECK_INT(played, MAX_GUESSES);
    CHECK_INT(game_guess(&game, "planet"), GUESS_GAME_OVER);
    wordlist_free(&list);
}

static void keyboard_keeps_the_best_colour(void)
{
    // Regression: the old blackenKeys ran before colouring and could turn a
    // known green letter grey.
    WordList list;
    REQUIRE(wordlist_from_array(&list, WORDS, NWORDS) == 0);
    Game game;
    game_start(&game, &list, "planet");
    CHECK_INT(game_guess(&game, "planer"), GUESS_OK);
    CHECK_INT(game.keys['e' - 'a'], KEY_CORRECT);
    CHECK_INT(game.keys['r' - 'a'], KEY_ABSENT);
    CHECK_INT(game.keys['z' - 'a'], KEY_UNKNOWN);
    // teeter: E at 4 green again, other E's grey, first T yellow.
    CHECK_INT(game_guess(&game, "teeter"), GUESS_OK);
    CHECK_INT(game.keys['e' - 'a'], KEY_CORRECT);
    CHECK_INT(game.keys['t' - 'a'], KEY_PRESENT);
    // muddly: L only yellow this time, but it was green before.
    CHECK_INT(game_guess(&game, "muddly"), GUESS_OK);
    CHECK_INT(game.keys['l' - 'a'], KEY_CORRECT);
    CHECK_INT(game.keys['m' - 'a'], KEY_ABSENT);
    wordlist_free(&list);
}

static void random_secrets_cover_the_whole_list(void)
{
    // The secret comes from the whole word list (no separate solutions
    // file any more), uniformly.
    WordList list;
    REQUIRE(wordlist_from_array(&list, WORDS, NWORDS) == 0);
    Rng rng;
    rng_seed(&rng, 1234);
    int seen[NWORDS] = { 0 };
    const int draws = 16000;
    for (int i = 0; i < draws; i++)
    {
        Game game;
        game_start_random(&game, &list, &rng);
        long index = wordlist_find(&list, game.secret);
        REQUIRE(index >= 0);
        seen[index]++;
    }
    for (size_t i = 0; i < NWORDS; i++)
        CHECK_NEAR(seen[i], draws / (int)NWORDS, 200);
    wordlist_free(&list);
}

void game_tests(void)
{
    RUN(guesses_are_validated);
    RUN(winning_ends_the_game);
    RUN(running_out_of_guesses_loses);
    RUN(keyboard_keeps_the_best_colour);
    RUN(random_secrets_cover_the_whole_list);
}
