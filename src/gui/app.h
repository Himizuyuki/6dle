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

// "Solver" tab: the network guesses, one step per click.
typedef struct SolvePage
{
    App *app;
    GtkWidget *widget;
    Game game;
    Solver solver;
    int ready; // a model is loaded and the solver initialised
    size_t left_after[MAX_GUESSES]; // possible words after each guess
    Board board;
    GtkWidget *secret;
    GtkWidget *entry;
    GtkWidget *step;
    GtkWidget *remaining;
    GtkWidget *chance;
    GtkWidget *status;
    GtkWidget *rank_num[SUGGESTIONS];
    GtkWidget *rank_word[SUGGESTIONS];
    GtkWidget *rank_bar[SUGGESTIONS];
    GtkWidget *rank_pct[SUGGESTIONS];
} SolvePage;

struct App
{
    WordList words;
    Mlp model;
    int has_model;
    const char *model_path;
    Rng rng;
    GtkWidget *window;
    GtkWidget *stack;
    PlayPage play;
    SolvePage solve;
};

GtkWidget *play_page_init(PlayPage *page, App *app);
void play_page_key(PlayPage *page, int key);
void play_page_free(PlayPage *page);

GtkWidget *solve_page_init(SolvePage *page, App *app);
void solve_page_free(SolvePage *page);

#endif // APP_H
