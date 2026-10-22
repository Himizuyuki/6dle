#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>

#include "board.h"
#include "core/game.h"
#include "core/rng.h"
#include "core/stats.h"
#include "core/words.h"
#include "keyboard.h"
#include "solver/nn.h"
#include "solver/solver.h"

#define SUGGESTIONS 5

typedef struct App App;

// "Play" tab: the user guesses, either today's daily word or free play.
typedef struct PlayPage
{
    App *app;
    GtkWidget *widget;
    Game free_game;
    PriorKind free_kind; // difficulty free_game was started at
    Game daily_game;
    Game *game; // the one on screen
    int daily; // showing the daily word
    long day; // puzzle number of daily_game (0: no daily available)
    char typed[WORD_LEN + 1];
    int ntyped;
    Board board;
    Keyboard keyboard;
    GtkWidget *mode_daily;
    GtkWidget *mode_free;
    GtkWidget *info;
    GtkWidget *status;
    GtkWidget *new_game;
    GtkWidget *share;
    guint flag_timeout;
} PlayPage;

// One page of the statistics popover.
typedef struct StatsView
{
    GtkWidget *played;
    GtkWidget *win;
    GtkWidget *streak;
    GtkWidget *best;
    GtkWidget *lost;
    GtkWidget *bar[MAX_GUESSES];
    GtkWidget *count[MAX_GUESSES];
} StatsView;

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
    Stats stats;
    char stats_path[4096]; // empty: statistics are not saved
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *stats_popover;
    GtkWidget *stats_stack;
    StatsView daily_view;
    StatsView free_view;
    PlayPage play;
    SolvePage solve;
};

// Weights to give the solver for a prior (NULL for uniform).
const float *app_prior_weights(const App *app, PriorKind kind);

// Statistics: the header bar button and its popover (stats_view.c).
GtkWidget *stats_button_new(App *app);
// Save the statistics and refresh the popover.
void app_stats_changed(App *app);
// Open the popover on the daily or the free-play page.
void app_stats_show(App *app, int daily);

GtkWidget *play_page_init(PlayPage *page, App *app);
void play_page_key(PlayPage *page, int key);
void play_page_free(PlayPage *page);

GtkWidget *solve_page_init(SolvePage *page, App *app);
void solve_page_free(SolvePage *page);

#endif // APP_H
