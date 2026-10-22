#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>

#include "board.h"
#include "core/game.h"
#include "core/rng.h"
#include "core/words.h"
#include "keyboard.h"
#include "solver/nn.h"
#include "solver/solver.h"

#define SUGGESTIONS 5

typedef struct App App;

// "Play" tab: the user guesses.
typedef struct PlayPage
{
    App *app;
    GtkWidget *widget;
    Game game;
    char typed[WORD_LEN + 1];
    int ntyped;
    Board board;
    Keyboard keyboard;
    GtkWidget *status;
    guint flag_timeout;
} PlayPage;

// Five ranked words with a probability bar each.
typedef struct RankList
{
    GtkWidget *num[SUGGESTIONS];
    GtkWidget *word[SUGGESTIONS];
    GtkWidget *bar[SUGGESTIONS];
    GtkWidget *pct[SUGGESTIONS];
    GtkWidget *tag[SUGGESTIONS];
} RankList;

// "Solver" tab: the network guesses, one step per click.
typedef struct SolvePage
{
    App *app;
    GtkWidget *widget;
    Game game;
    PriorKind prior; // difficulty of the current game
    Solver solver;
    int ready; // a model is loaded and the solver initialised
    size_t left_after[MAX_GUESSES]; // possible words after each guess
    Board board;
    GtkWidget *secret;
    GtkWidget *secret_kind;
    GtkWidget *entry;
    GtkWidget *step;
    GtkWidget *probes;
    GtkWidget *remaining;
    GtkWidget *remaining_label;
    GtkWidget *status;
    RankList answers; // most likely secrets (exact posterior)
    RankList guesses; // network's next guess
} SolvePage;

struct App
{
    WordList words;
    Prior priors[PRIOR_KINDS]; // indexed by PriorKind
    int has_freq; // word frequencies loaded: difficulty levels available
    PriorKind prior; // difficulty of new games
    Mlp model;
    int has_model;
    const char *model_path;
    Rng rng;
    GtkWidget *window;
    GtkWidget *stack;
    PlayPage play;
    SolvePage solve;
};

// Weights to give the solver for a prior (NULL for uniform).
const float *app_prior_weights(const App *app, PriorKind kind);

GtkWidget *play_page_init(PlayPage *page, App *app);
void play_page_key(PlayPage *page, int key);
void play_page_free(PlayPage *page);

GtkWidget *solve_page_init(SolvePage *page, App *app);
void solve_page_free(SolvePage *page);

#endif // APP_H
