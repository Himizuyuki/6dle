#include <ctype.h>

#include "app.h"
#include "style.h"

static void refresh(PlayPage *page)
{
    board_show(&page->board, &page->game, page->typed);
    keyboard_show(&page->keyboard, page->game.keys);
}

static void set_status(PlayPage *page, const char *text)
{
    gtk_label_set_text(GTK_LABEL(page->status), text);
}

static gboolean clear_flag(gpointer data)
{
    PlayPage *page = data;
    page->flag_timeout = 0;
    refresh(page);
    return G_SOURCE_REMOVE;
}

// Red outline on the row being typed, for a rejected word.
static void flag_current_row(PlayPage *page, const char *message)
{
    set_status(page, message);
    board_flag_row(&page->board, page->game.nguesses);
    if (page->flag_timeout != 0)
        g_source_remove(page->flag_timeout);
    page->flag_timeout = g_timeout_add(700, clear_flag, page);
}

static void new_game(PlayPage *page)
{
    App *app = page->app;
    game_start_random(&page->game, &app->words, &app->priors[app->prior],
                      &app->rng);
    page->typed[0] = '\0';
    page->ntyped = 0;
    set_status(page, "Guess the six letter word");
    refresh(page);
}

static void submit(PlayPage *page)
{
    if (page->ntyped < WORD_LEN)
    {
        flag_current_row(page, guess_result_message(GUESS_BAD_LENGTH));
        return;
    }
    GuessResult result = game_guess(&page->game, page->typed);
    if (result != GUESS_OK)
    {
        flag_current_row(page, guess_result_message(result));
        return;
    }
    page->typed[0] = '\0';
    page->ntyped = 0;
    refresh(page);

    char text[96];
    if (page->game.status == GAME_WON)
        g_snprintf(text, sizeof(text),
                   "Congrats, you got it in %d! Press Enter to play again.",
                   page->game.nguesses);
    else if (page->game.status == GAME_LOST)
    {
        char secret[WORD_LEN + 1];
        for (int i = 0; i <= WORD_LEN; i++)
            secret[i] = (char)toupper((unsigned char)page->game.secret[i]);
        g_snprintf(text, sizeof(text),
                   "The word was %s. Press Enter to play again.", secret);
    }
    else
        g_snprintf(text, sizeof(text), "%d guesses left",
                   MAX_GUESSES - page->game.nguesses);
    set_status(page, text);
}

void play_page_key(PlayPage *page, int key)
{
    if (page->game.status != GAME_PLAYING)
    {
        if (key == KEY_ENTER)
            new_game(page);
        return;
    }
    if (key == KEY_ENTER)
    {
        submit(page);
        return;
    }
    if (key >= 'a' && key <= 'z' && page->ntyped < WORD_LEN)
    {
        page->typed[page->ntyped++] = (char)key;
        page->typed[page->ntyped] = '\0';
    }
    else if (key == KEY_BACKSPACE && page->ntyped > 0)
        page->typed[--page->ntyped] = '\0';
    else
        return;
    refresh(page);
}

static void on_key(void *user, int key)
{
    play_page_key(user, key);
}

static void on_new_game(GtkButton *button, PlayPage *page)
{
    (void)button;
    new_game(page);
}

GtkWidget *play_page_init(PlayPage *page, App *app)
{
    page->app = app;
    page->flag_timeout = 0;
    page->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(page->widget, 18);
    gtk_widget_set_margin_bottom(page->widget, 18);
    gtk_widget_set_halign(page->widget, GTK_ALIGN_CENTER);

    page->status = gtk_label_new("");
    style_add(page->status, "status");
    gtk_container_add(GTK_CONTAINER(page->widget), page->status);

    board_init(&page->board, 0);
    gtk_container_add(GTK_CONTAINER(page->widget), page->board.widget);

    keyboard_init(&page->keyboard, on_key, page);
    gtk_widget_set_margin_top(page->keyboard.widget, 8);
    gtk_container_add(GTK_CONTAINER(page->widget), page->keyboard.widget);

    GtkWidget *button = gtk_button_new_with_label("New game");
    gtk_widget_set_can_focus(button, FALSE);
    gtk_widget_set_halign(button, GTK_ALIGN_CENTER);
    g_signal_connect(button, "clicked", G_CALLBACK(on_new_game), page);
    gtk_container_add(GTK_CONTAINER(page->widget), button);

    new_game(page);
    return page->widget;
}

void play_page_free(PlayPage *page)
{
    if (page->flag_timeout != 0)
        g_source_remove(page->flag_timeout);
    page->flag_timeout = 0;
}
