#include <ctype.h>

#include "app.h"
#include "style.h"

static void set_upper(char *dst, const char *src)
{
    for (int i = 0; i < WORD_LEN; i++)
        dst[i] = (char)toupper((unsigned char)src[i]);
    dst[WORD_LEN] = '\0';
}

static void format_percent(char *buf, size_t size, double p)
{
    if (p >= 0.9995)
        g_snprintf(buf, size, "100%%");
    else if (p >= 0.001)
        g_snprintf(buf, size, "%.1f%%", 100.0 * p);
    else
        g_snprintf(buf, size, "<0.1%%");
}

static void show_rankings(SolvePage *page)
{
    Suggestion top[SUGGESTIONS];
    size_t n = page->ready ? solver_top(&page->solver, top, SUGGESTIONS) : 0;
    for (size_t i = 0; i < SUGGESTIONS; i++)
    {
        gboolean visible = i < n;
        gtk_widget_set_visible(page->rank_num[i], visible);
        gtk_widget_set_visible(page->rank_word[i], visible);
        gtk_widget_set_visible(page->rank_bar[i], visible);
        gtk_widget_set_visible(page->rank_pct[i], visible);
        if (!visible)
            continue;
        char word[WORD_LEN + 1];
        char pct[16];
        set_upper(word, page->app->words.items[top[i].word]);
        format_percent(pct, sizeof(pct), top[i].prob);
        gtk_label_set_text(GTK_LABEL(page->rank_word[i]), word);
        gtk_label_set_text(GTK_LABEL(page->rank_pct[i]), pct);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(page->rank_bar[i]),
                                      top[i].prob);
    }
}

static void refresh(SolvePage *page)
{
    const Game *game = &page->game;
    board_show(&page->board, game, NULL);

    char text[128];
    for (int r = 0; r < MAX_GUESSES; r++)
    {
        text[0] = '\0';
        if (r < game->nguesses && game->patterns[r] == PATTERN_WIN)
            g_snprintf(text, sizeof(text), "solved!");
        else if (r < game->nguesses)
            g_snprintf(text, sizeof(text), "%zu left", page->left_after[r]);
        board_set_note(&page->board, r, text);
    }

    char secret[WORD_LEN + 1];
    set_upper(secret, game->secret);
    gtk_label_set_text(GTK_LABEL(page->secret), secret);

    if (!page->ready)
    {
        g_snprintf(text, sizeof(text),
                   "No solver model found at\n%s\nRun `make train` to "
                   "create it.",
                   page->app->model_path);
        gtk_label_set_text(GTK_LABEL(page->status), text);
        gtk_widget_set_sensitive(page->step, FALSE);
        show_rankings(page);
        return;
    }

    size_t left = solver_candidates(&page->solver);
    g_snprintf(text, sizeof(text), "%zu", left);
    gtk_label_set_text(GTK_LABEL(page->remaining), text);
    if (left == 1)
        g_snprintf(text, sizeof(text), "word left: it must be the secret");
    else
    {
        // Secrets are drawn uniformly, so every possible word is equally
        // likely to be the answer.
        char pct[16];
        format_percent(pct, sizeof(pct), left ? 1.0 / (double)left : 0.0);
        g_snprintf(text, sizeof(text),
                   "words still possible\n(each one is the secret with %s "
                   "chance)",
                   pct);
    }
    gtk_label_set_text(GTK_LABEL(page->chance), text);
    show_rankings(page);

    if (game->status == GAME_WON)
        g_snprintf(text, sizeof(text), "Solved in %d guess%s.", game->nguesses,
                   game->nguesses == 1 ? "" : "es");
    else if (game->status == GAME_LOST)
        g_snprintf(text, sizeof(text), "Out of guesses.");
    else if (left == 0)
        g_snprintf(text, sizeof(text), "No word in the list fits.");
    else
        g_snprintf(text, sizeof(text), "Guess %d of %d", game->nguesses + 1,
                   MAX_GUESSES);
    gtk_label_set_text(GTK_LABEL(page->status), text);
    gtk_widget_set_sensitive(page->step,
                             game->status == GAME_PLAYING && left > 0);
}

static void on_step(GtkButton *button, SolvePage *page)
{
    (void)button;
    if (!page->ready || page->game.status != GAME_PLAYING)
        return;
    uint32_t best = solver_best(&page->solver);
    if (best == SOLVER_NO_WORD)
        return;
    const char *guess = page->app->words.items[best];
    if (game_guess(&page->game, guess) != GUESS_OK)
        return;
    int row = page->game.nguesses - 1;
    solver_apply(&page->solver, guess, page->game.patterns[row]);
    page->left_after[row] = solver_candidates(&page->solver);
    refresh(page);
}

static void on_new_word(GtkWidget *widget, SolvePage *page)
{
    (void)widget;
    App *app = page->app;
    const char *text = gtk_entry_get_text(GTK_ENTRY(page->entry));
    if (text[0] != '\0')
    {
        long index = wordlist_find(&app->words, text);
        if (index < 0)
        {
            gtk_label_set_text(GTK_LABEL(page->status),
                               "That word is not in the word list.");
            return;
        }
        game_start(&page->game, &app->words, app->words.items[index]);
        gtk_entry_set_text(GTK_ENTRY(page->entry), "");
    }
    else
        game_start_random(&page->game, &app->words, &app->rng);
    if (page->ready)
        solver_reset(&page->solver);
    refresh(page);
}

static GtkWidget *label(const char *text, const char *css_class)
{
    GtkWidget *l = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
    if (css_class != NULL)
        style_add(l, css_class);
    return l;
}

static GtkWidget *build_panel(SolvePage *page)
{
    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    style_add(panel, "panel");
    gtk_widget_set_size_request(panel, 320, -1);
    gtk_widget_set_valign(panel, GTK_ALIGN_START);

    gtk_container_add(GTK_CONTAINER(panel),
                      label("Neural network solver", "panel-title"));

    GtkWidget *secret_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_add(GTK_CONTAINER(secret_row), label("Secret word", "dim"));
    page->secret = label("", "secret");
    gtk_container_add(GTK_CONTAINER(secret_row), page->secret);
    gtk_container_add(GTK_CONTAINER(panel), secret_row);

    page->remaining = label("", "panel-big");
    gtk_widget_set_margin_top(page->remaining, 6);
    gtk_container_add(GTK_CONTAINER(panel), page->remaining);
    page->chance = label("", "dim");
    gtk_container_add(GTK_CONTAINER(panel), page->chance);

    GtkWidget *heading = label("Most likely next guesses", "panel-title");
    gtk_widget_set_margin_top(heading, 10);
    gtk_container_add(GTK_CONTAINER(panel), heading);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    for (int i = 0; i < SUGGESTIONS; i++)
    {
        char rank[8];
        g_snprintf(rank, sizeof(rank), "%d.", i + 1);
        page->rank_num[i] = label(rank, "dim");
        gtk_grid_attach(GTK_GRID(grid), page->rank_num[i], 0, i, 1, 1);
        page->rank_word[i] = label("", "rank-word");
        gtk_grid_attach(GTK_GRID(grid), page->rank_word[i], 1, i, 1, 1);
        page->rank_bar[i] = gtk_progress_bar_new();
        if (i == 0)
            style_add(page->rank_bar[i], "best");
        gtk_widget_set_hexpand(page->rank_bar[i], TRUE);
        gtk_widget_set_valign(page->rank_bar[i], GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(grid), page->rank_bar[i], 2, i, 1, 1);
        page->rank_pct[i] = label("", "rank-pct");
        gtk_label_set_xalign(GTK_LABEL(page->rank_pct[i]), 1.0f);
        gtk_label_set_width_chars(GTK_LABEL(page->rank_pct[i]), 6);
        gtk_grid_attach(GTK_GRID(grid), page->rank_pct[i], 3, i, 1, 1);
        // Visibility is driven by show_rankings, not gtk_widget_show_all.
        gtk_widget_set_no_show_all(page->rank_num[i], TRUE);
        gtk_widget_set_no_show_all(page->rank_word[i], TRUE);
        gtk_widget_set_no_show_all(page->rank_bar[i], TRUE);
        gtk_widget_set_no_show_all(page->rank_pct[i], TRUE);
    }
    gtk_container_add(GTK_CONTAINER(panel), grid);

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(buttons, 14);
    page->step = gtk_button_new_with_label("Step ▶");
    style_add(page->step, "suggested-action");
    gtk_widget_set_hexpand(page->step, TRUE);
    g_signal_connect(page->step, "clicked", G_CALLBACK(on_step), page);
    gtk_container_add(GTK_CONTAINER(buttons), page->step);
    GtkWidget *new_word = gtk_button_new_with_label("New word");
    gtk_widget_set_hexpand(new_word, TRUE);
    g_signal_connect(new_word, "clicked", G_CALLBACK(on_new_word), page);
    gtk_container_add(GTK_CONTAINER(buttons), new_word);
    gtk_container_add(GTK_CONTAINER(panel), buttons);

    page->entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(page->entry), WORD_LEN);
    gtk_entry_set_placeholder_text(GTK_ENTRY(page->entry),
                                   "Secret for the next word (optional)");
    g_signal_connect(page->entry, "activate", G_CALLBACK(on_new_word), page);
    gtk_container_add(GTK_CONTAINER(panel), page->entry);

    page->status = label("", "status");
    gtk_label_set_line_wrap(GTK_LABEL(page->status), TRUE);
    gtk_widget_set_margin_top(page->status, 6);
    gtk_container_add(GTK_CONTAINER(panel), page->status);
    return panel;
}

GtkWidget *solve_page_init(SolvePage *page, App *app)
{
    page->app = app;
    page->ready = app->has_model
        && solver_init(&page->solver, &app->words, &app->model) == 0;

    page->widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_widget_set_margin_top(page->widget, 18);
    gtk_widget_set_margin_bottom(page->widget, 18);
    gtk_widget_set_margin_start(page->widget, 18);
    gtk_widget_set_margin_end(page->widget, 18);
    gtk_widget_set_halign(page->widget, GTK_ALIGN_CENTER);

    board_init(&page->board, 1);
    gtk_container_add(GTK_CONTAINER(page->widget), page->board.widget);
    gtk_container_add(GTK_CONTAINER(page->widget), build_panel(page));

    game_start_random(&page->game, &app->words, &app->rng);
    refresh(page);
    return page->widget;
}

void solve_page_free(SolvePage *page)
{
    if (page->ready)
        solver_free(&page->solver);
    page->ready = 0;
}
