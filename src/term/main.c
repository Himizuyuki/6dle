/*
 * Terminal front end (the original 6dle interface).
 *   6dle-term                 play in the terminal
 *   6dle-term --solve [WORD]  watch the neural network solve WORD (or a
 *                             random word), printing the most likely
 *                             secrets and its top guesses
 * --difficulty easy|normal|hard|any chooses how the secret is picked,
 * --hard keeps the solver to words that can still be the secret.
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>

#include "core/game.h"
#include "solver/nn.h"
#include "solver/solver.h"

#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define RED "\033[1;31m"
#define DARK "\033[1;30m"
#define RESET "\033[0m"
#define TOP_SHOWN 5

static void clear_screen(void)
{
    printf("\033[H\033[2J");
}

// Read one key without waiting for Enter or echoing it.
static int getch(void)
{
    struct termios old;
    if (tcgetattr(STDIN_FILENO, &old) != 0)
        return getchar();
    struct termios raw = old;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    int c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    return c;
}

static const char *mark_color(Mark m)
{
    return m == MARK_CORRECT ? GREEN : m == MARK_PRESENT ? YELLOW : "";
}

static void print_row(const char *letters, const Mark *marks, int flagged,
                      int cursor)
{
    for (int i = 0; i < WORD_LEN; i++)
    {
        char c = letters[i] ? (char)toupper((unsigned char)letters[i]) : ' ';
        printf("║ %s%c" RESET " ",
               flagged     ? RED
                   : marks ? mark_color(marks[i])
                           : "",
               c);
    }
    printf(cursor ? "║«\n" : "║\n");
}

static void print_board(const Game *game, const char *typed, int flagged)
{
    printf("╔═══════════════════════╗\n");
    printf("║You have %d guesses left║\n", MAX_GUESSES - game->nguesses);
    printf("╠═══╦═══╦═══╦═══╦═══╦═══╣\n");
    for (int r = 0; r < MAX_GUESSES; r++)
    {
        char empty[WORD_LEN + 1] = { 0 };
        if (r < game->nguesses)
        {
            Mark marks[WORD_LEN];
            pattern_to_marks(game->patterns[r], marks);
            print_row(game->guesses[r], marks, 0, 0);
        }
        else if (r == game->nguesses && typed != NULL)
        {
            strncpy(empty, typed, WORD_LEN);
            print_row(empty, NULL, flagged, 1);
        }
        else
            print_row(empty, NULL, 0, 0);
        printf(r == MAX_GUESSES - 1 ? "╚═══╩═══╩═══╩═══╩═══╩═══╝\n"
                                    : "╠═══╬═══╬═══╬═══╬═══╬═══╣\n");
    }
}

static void print_keys(const Game *game)
{
    static const char *const ROWS[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
    static const char *const INDENT[] = { "   ", "    ", "      " };
    for (int r = 0; r < 3; r++)
    {
        printf("%s", INDENT[r]);
        for (const char *c = ROWS[r]; *c != '\0'; c++)
        {
            KeyState k = game->keys[*c - 'a'];
            const char *color = k == KEY_CORRECT ? GREEN
                : k == KEY_PRESENT               ? YELLOW
                : k == KEY_ABSENT                ? DARK
                                                 : "";
            printf("%s%c " RESET, color, *c);
        }
        printf("\n");
    }
}

static void draw(const Game *game, const char *typed, int flagged,
                 const char *message)
{
    clear_screen();
    print_board(game, typed, flagged);
    print_keys(game);
    if (message != NULL && message[0] != '\0')
        printf("%s\n", message);
    fflush(stdout);
}

// Returns 0 if the player wants to stop (or input ended).
static int play_round(Game *game)
{
    char typed[WORD_LEN + 1] = { 0 };
    int len = 0;
    const char *message = NULL;
    while (game->status == GAME_PLAYING)
    {
        draw(game, typed, 0, message);
        message = NULL;
        int c = getch();
        if (c == EOF)
            return 0;
        if (isalpha(c) && len < WORD_LEN)
            typed[len++] = (char)tolower(c);
        else if ((c == 127 || c == '\b') && len > 0)
            typed[--len] = '\0';
        else if (c == '\n' && len == WORD_LEN)
        {
            GuessResult result = game_guess(game, typed);
            if (result == GUESS_OK)
            {
                memset(typed, 0, sizeof(typed));
                len = 0;
            }
            else
            {
                draw(game, typed, 1, guess_result_message(result));
                sleep(1);
            }
        }
    }

    draw(game, NULL, 0, NULL);
    if (game->status == GAME_WON)
    {
        printf("╔═══════════════════════╗\n");
        printf("║ Congrats you got it ! ║\n");
        printf("╚═══════════════════════╝\n");
    }
    else
    {
        printf("╔═══════════════════════╗\n");
        printf("║     The word was      ║\n");
        printf("║        %s         ║\n", game->secret);
        printf("╚═══════════════════════╝\n");
    }
    printf("╔═══════════════════════╗\n");
    printf("║Want to play some more?║\n");
    printf("║     (Y)es | (n)o      ║\n");
    printf("╚═══════════════════════╝\n");
    fflush(stdout);
    for (;;)
    {
        int c = getch();
        if (c == 'y' || c == 'Y' || c == '\n')
            return 1;
        if (c == 'n' || c == 'N' || c == EOF)
            return 0;
    }
}

static int play(const WordList *words, const Prior *prior)
{
    Rng rng;
    rng_seed_random(&rng);
    Game game;
    do
        game_start_random(&game, words, prior, &rng);
    while (play_round(&game));
    printf("Thanks for playing!\n");
    return EXIT_SUCCESS;
}

static void print_guess(const char *guess, Pattern p)
{
    Mark marks[WORD_LEN];
    pattern_to_marks(p, marks);
    for (int i = 0; i < WORD_LEN; i++)
    {
        const char *color = marks[i] == MARK_CORRECT ? GREEN
            : marks[i] == MARK_PRESENT               ? YELLOW
                                                     : DARK;
        printf("%s%c" RESET, color, toupper((unsigned char)guess[i]));
    }
}

static void print_list(const char *title, const WordList *words,
                       const Solver *solver, const Suggestion *top, size_t n)
{
    printf("  %-20s", title);
    for (size_t i = 0; i < n; i++)
        printf(" %s%s %.1f%%", words->items[top[i].word],
               solver_is_candidate(solver, top[i].word) ? "" : "*",
               100.0 * (double)top[i].prob);
    printf("\n");
}

static int solve(const WordList *words, const Prior *prior,
                 const char *model_path, const char *secret, int probes)
{
    Mlp model;
    if (mlp_load(&model, model_path) != 0)
    {
        fprintf(stderr, "6dle-term: cannot load %s (run `make train`)\n",
                model_path);
        return EXIT_FAILURE;
    }
    Solver solver;
    const float *weights = prior->kind == PRIOR_UNIFORM ? NULL : prior->weight;
    if (solver_init(&solver, words, &model, weights) != 0)
    {
        fprintf(stderr, "6dle-term: %s does not fit this solver\n", model_path);
        mlp_free(&model);
        return EXIT_FAILURE;
    }
    solver_set_probes(&solver, probes);

    Game game;
    if (secret != NULL)
    {
        if (wordlist_find(words, secret) < 0)
        {
            fprintf(stderr, "6dle-term: %s is not in the word list\n", secret);
            solver_free(&solver);
            mlp_free(&model);
            return EXIT_FAILURE;
        }
        game_start(&game, words, secret);
    }
    else
    {
        Rng rng;
        rng_seed_random(&rng);
        game_start_random(&game, words, prior, &rng);
    }

    printf("Secret word: %s (difficulty: %s, %s)\n\n", game.secret,
           prior_name(prior->kind), probes ? "probes allowed" : "hard mode");
    while (game.status == GAME_PLAYING)
    {
        Suggestion top[TOP_SHOWN];
        printf("%zu possible words\n", solver_candidates(&solver));
        size_t n = solver_top_answers(&solver, top, TOP_SHOWN);
        print_list("most likely secret:", words, &solver, top, n);
        n = solver_top(&solver, top, TOP_SHOWN);
        if (n == 0)
            break;
        print_list("next guess:", words, &solver, top, n);

        const char *guess = words->items[top[0].word];
        game_guess(&game, guess);
        Pattern p = game.patterns[game.nguesses - 1];
        solver_apply(&solver, guess, p);
        printf("  guess %d: ", game.nguesses);
        print_guess(guess, p);
        printf("\n\n");
    }
    if (game.status == GAME_WON)
        printf("Found %s in %d guesses\n", game.secret, game.nguesses);
    else
        printf("Could not find %s in %d guesses\n", game.secret, game.nguesses);
    printf("(* = probe: a word that can't be the secret)\n");

    solver_free(&solver);
    mlp_free(&model);
    return game.status == GAME_WON ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [options] [--solve [WORD]]\n"
            "  --words FILE   word list (default %s)\n"
            "  --freq FILE    word frequencies (default %s)\n"
            "  --model FILE   solver network (default %s)\n"
            "  --difficulty L how the secret is picked: easy, normal "
            "(default),\n"
            "                 hard or any (all words equally likely)\n"
            "  --hard         solver: only guess words that can be the "
            "secret\n"
            "  --solve [WORD] let the solver find WORD (default: random)\n",
            prog, SIXDLE_WORDS, SIXDLE_FREQ, SIXDLE_MODEL);
}

int main(int argc, char **argv)
{
    const char *words_path = SIXDLE_WORDS;
    const char *freq_path = SIXDLE_FREQ;
    const char *model_path = SIXDLE_MODEL;
    int solving = 0;
    int probes = 1;
    PriorKind kind = PRIOR_NORMAL;
    const char *secret = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--words") == 0 && i + 1 < argc)
            words_path = argv[++i];
        else if (strcmp(argv[i], "--freq") == 0 && i + 1 < argc)
            freq_path = argv[++i];
        else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc)
            model_path = argv[++i];
        else if (strcmp(argv[i], "--difficulty") == 0 && i + 1 < argc)
        {
            const char *level = argv[++i];
            int found = 0;
            for (int p = 0; p < PRIOR_KINDS && !found; p++)
            {
                if (strncasecmp(level, prior_name((PriorKind)p), strlen(level))
                        == 0
                    && level[0] != '\0')
                {
                    kind = (PriorKind)p;
                    found = 1;
                }
            }
            if (!found)
            {
                usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(argv[i], "--hard") == 0)
            probes = 0;
        else if (strcmp(argv[i], "--solve") == 0)
        {
            solving = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-')
                secret = argv[++i];
        }
        else
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    WordList words;
    errno = 0;
    if (wordlist_load(&words, words_path) != 0)
    {
        fprintf(stderr, "6dle-term: cannot load word list %s: %s\n", words_path,
                errno ? strerror(errno) : "no six letter words");
        return EXIT_FAILURE;
    }
    if (kind != PRIOR_UNIFORM
        && wordlist_load_frequencies(&words, freq_path) != 0)
    {
        fprintf(stderr,
                "6dle-term: no word frequencies at %s, any word can "
                "be the secret\n",
                freq_path);
        kind = PRIOR_UNIFORM;
    }
    Prior prior;
    if (prior_init(&prior, &words, kind) != 0)
        return EXIT_FAILURE;
    int status = solving ? solve(&words, &prior, model_path, secret, probes)
                         : play(&words, &prior);
    prior_free(&prior);
    wordlist_free(&words);
    return status;
}
