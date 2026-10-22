#include <ctype.h>

#include "app.h"
#include "core/daily.h"
#include "style.h"

static void refresh(PlayPage *page)
{
    board_show(&page->board, page->game, page->typed);
    keyboard_show(&page->keyboard, page->game->keys);
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
    board_flag_row(&page->board, page->game->nguesses);
    if (page->flag_timeout != 0)
        g_source_remove(page->flag_timeout);
    page->flag_timeout = g_timeout_add(700, clear_flag, page);
}

// Status line, info line and buttons for the game on screen.
static void show_state(PlayPage *page)
{
    const Game *game = page->game;
    char text[160];
    char secret[WORD_LEN + 1];
    for (int i = 0; i <= WORD_LEN; i++)
        secret[i] = (char)toupper((unsigned char)game->secret[i]);
    long left = daily_seconds_left();
    char next[48];
    g_snprintf(next, sizeof(next), "Next word in %ld h %02ld min.", left / 3600,
               left % 3600 / 60);

    if (game->status == GAME_PLAYING)
    {
        if (game->nguesses == 0)
            g_snprintf(text, sizeof(text), "Guess the six letter word");
        else
            g_snprintf(text, sizeof(text), "%d guesses left",
                       MAX_GUESSES - game->nguesses);
    }
    else if (page->daily && game->status == GAME_WON)
        g_snprintf(text, sizeof(text), "Solved #%ld in %d! %s", page->day,
                   game->nguesses, next);
    else if (page->daily)
        g_snprintf(text, sizeof(text), "The word was %s. %s", secret, next);
    else if (game->status == GAME_WON)
        g_snprintf(text, sizeof(text),
                   "Congrats, you got it in %d! Press Enter to play again.",
                   game->nguesses);
    else
        g_snprintf(text, sizeof(text),
                   "The word was %s. Press Enter to play again.", secret);
    set_status(page, text);

    if (page->daily)
        g_snprintf(text, sizeof(text),
                   "Puzzle #%ld: one word a day, the same for everyone",
                   page->day);
    else
        g_snprintf(text, sizeof(text), "Free play, difficulty: %s",
                   prior_name(page->free_kind));
    gtk_label_set_text(GTK_LABEL(page->info), text);
    gtk_widget_set_visible(page->new_game, !page->daily);
    gtk_widget_set_visible(page->share,
                           page->daily && game->status != GAME_PLAYING);
}

// Today's daily word, with the guesses already made today.
static void load_daily(PlayPage *page)
{
    App *app = page->app;
    long today = daily_today();
    long word = daily_word(&app->words, today);
    page->day = word >= 0 ? today : 0;
    if (word < 0)
        return;
    game_start(&page->daily_game, &app->words, app->words.items[word]);
    if (app->stats.progress_day == today)
    {
        for (int i = 0; i < app->stats.nprogress; i++)
            game_guess(&page->daily_game, app->stats.progress[i]);
    }
    char label[32];
    g_snprintf(label, sizeof(label), "Daily #%ld", today);
    gtk_button_set_label(GTK_BUTTON(page->mode_daily), label);
}

static void set_mode(PlayPage *page, int daily)
{
    // A new day may have started since the daily was loaded.
    if (daily && page->day != 0 && page->day != daily_today())
        load_daily(page);
    page->daily = daily && page->day != 0;
    page->game = page->daily ? &page->daily_game : &page->free_game;
    page->typed[0] = '\0';
    page->ntyped = 0;
    show_state(page);
    refresh(page);
}

static void new_game(PlayPage *page)
{
    App *app = page->app;
    page->free_kind = app->prior;
    game_start_random(&page->free_game, &app->words, &app->priors[app->prior],
                      &app->rng);
    set_mode(page, 0);
}

static void submit(PlayPage *page)
{
    if (page->ntyped < WORD_LEN)
    {
        flag_current_row(page, guess_result_message(GUESS_BAD_LENGTH));
        return;
    }
    GuessResult result = game_guess(page->game, page->typed);
    if (result != GUESS_OK)
    {
        flag_current_row(page, guess_result_message(result));
        return;
    }
    page->typed[0] = '\0';
    page->ntyped = 0;
    refresh(page);
    show_state(page);

    App *app = page->app;
    int over = page->game->status != GAME_PLAYING;
    if (page->daily)
    {
        // Saved after every guess: quitting doesn't give a second try.
        stats_set_progress(&app->stats, page->game, page->day);
        if (over)
            stats_record_daily(&app->stats, page->game, page->day);
        app_stats_changed(app);
        if (over)
            app_stats_show(app, 1);
    }
    else if (over)
    {
        stats_record_free(&app->stats, page->game);
        app_stats_changed(app);
    }
}

void play_page_key(PlayPage *page, int key)
{
    if (page->game->status != GAME_PLAYING)
    {
        if (key == KEY_ENTER && !page->daily)
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

static void on_share(GtkButton *button, PlayPage *page)
{
    (void)button;
    char text[512];
    daily_share(page->game, page->day, text, sizeof(text));
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), text,
                           -1);
    set_status(page, "Result copied: paste it anywhere to share it.");
}

static void on_mode(GtkToggleButton *button, PlayPage *page)
{
    if (gtk_toggle_button_get_active(button))
        set_mode(page, GTK_WIDGET(button) == page->mode_daily);
}

static GtkWidget *mode_button(GtkWidget *group, const char *label)
{
    GtkWidget *button = group == NULL
        ? gtk_radio_button_new_with_label(NULL, label)
        : gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(group),
                                                      label);
    // Drawn as a toggle button rather than a radio circle.
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_widget_set_can_focus(button, FALSE);
    return button;
}

static GtkWidget *bottom_button(const char *label, GCallback callback,
                                PlayPage *page)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_widget_set_can_focus(button, FALSE);
    gtk_widget_set_no_show_all(button, TRUE); // shown by show_state
    g_signal_connect(button, "clicked", callback, page);
    return button;
}

GtkWidget *play_page_init(PlayPage *page, App *app)
{
    page->app = app;
    page->flag_timeout = 0;
    page->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(page->widget, 14);
    gtk_widget_set_margin_bottom(page->widget, 18);
    gtk_widget_set_halign(page->widget, GTK_ALIGN_CENTER);

    GtkWidget *modes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    style_add(modes, "linked");
    style_add(modes, "mode-switch");
    gtk_widget_set_halign(modes, GTK_ALIGN_CENTER);
    page->mode_daily = mode_button(NULL, "Daily");
    page->mode_free = mode_button(page->mode_daily, "Free play");
    gtk_container_add(GTK_CONTAINER(modes), page->mode_daily);
    gtk_container_add(GTK_CONTAINER(modes), page->mode_free);
    gtk_container_add(GTK_CONTAINER(page->widget), modes);

    page->info = gtk_label_new("");
    style_add(page->info, "dim");
    gtk_container_add(GTK_CONTAINER(page->widget), page->info);

    page->status = gtk_label_new("");
    style_add(page->status, "status");
    gtk_container_add(GTK_CONTAINER(page->widget), page->status);

    board_init(&page->board, 0);
    gtk_container_add(GTK_CONTAINER(page->widget), page->board.widget);

    keyboard_init(&page->keyboard, on_key, page);
    gtk_widget_set_margin_top(page->keyboard.widget, 8);
    gtk_container_add(GTK_CONTAINER(page->widget), page->keyboard.widget);

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttons, GTK_ALIGN_CENTER);
    page->new_game = bottom_button("New game", G_CALLBACK(on_new_game), page);
    page->share = bottom_button("Copy result", G_CALLBACK(on_share), page);
    style_add(page->share, "suggested-action");
    gtk_container_add(GTK_CONTAINER(buttons), page->new_game);
    gtk_container_add(GTK_CONTAINER(buttons), page->share);
    gtk_container_add(GTK_CONTAINER(page->widget), buttons);

    page->free_kind = app->prior;
    game_start_random(&page->free_game, &app->words, &app->priors[app->prior],
                      &app->rng);
    load_daily(page);
    if (page->day == 0)
    {
        gtk_widget_set_sensitive(page->mode_daily, FALSE);
        gtk_widget_set_tooltip_text(page->mode_daily,
                                    "Needs the word frequencies "
                                    "(data/wordFreq.txt)");
    }
    // Open on the daily word until it is done for the day.
    int daily = page->day != 0 && page->daily_game.status == GAME_PLAYING;
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(daily ? page->mode_daily : page->mode_free), TRUE);
    g_signal_connect(page->mode_daily, "toggled", G_CALLBACK(on_mode), page);
    g_signal_connect(page->mode_free, "toggled", G_CALLBACK(on_mode), page);
    set_mode(page, daily);
    return page->widget;
}

void play_page_free(PlayPage *page)
{
    if (page->flag_timeout != 0)
        g_source_remove(page->flag_timeout);
    page->flag_timeout = 0;
}
