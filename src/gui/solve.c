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

// Fill a ranked list; probe words (not possible answers) get a tag.
static void show_list(SolvePage *page, RankList *list, const Suggestion *top,
                      size_t n, int tag_probes)
{
    for (size_t i = 0; i < SUGGESTIONS; i++)
    {
        gboolean visible = i < n;
        gtk_widget_set_visible(list->num[i], visible);
        gtk_widget_set_visible(list->word[i], visible);
        gtk_widget_set_visible(list->bar[i], visible);
        gtk_widget_set_visible(list->pct[i], visible);
        gtk_widget_set_visible(
            list->tag[i],
            visible && tag_probes
                && !solver_is_candidate(&page->solver, top[i].word));
        if (!visible)
            continue;
        char word[WORD_LEN + 1];
        char pct[16];
        set_upper(word, page->app->words.items[top[i].word]);
        format_percent(pct, sizeof(pct), top[i].prob);
        gtk_label_set_text(GTK_LABEL(list->word[i]), word);
        gtk_label_set_text(GTK_LABEL(list->pct[i]), pct);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(list->bar[i]),
                                      top[i].prob);
    }
}

static void refresh(SolvePage *page)
{
    const Game *game = &page->game;
    board_show(&page->board, game, NULL);

    char text[160];
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
    g_snprintf(text, sizeof(text), "(%s)", prior_name(page->prior));
    gtk_label_set_text(GTK_LABEL(page->secret_kind), text);

    if (!page->ready)
    {
        g_snprintf(text, sizeof(text),
                   "No solver model found at\n%s\nRun `make train` to "
                   "create it.",
                   page->app->model_path);
        gtk_label_set_text(GTK_LABEL(page->status), text);
        gtk_widget_set_sensitive(page->step, FALSE);
        show_list(page, &page->answers, NULL, 0, 0);
        show_list(page, &page->guesses, NULL, 0, 0);
        return;
    }

    size_t left = solver_candidates(&page->solver);
    g_snprintf(text, sizeof(text), "%zu", left);
    gtk_label_set_text(GTK_LABEL(page->remaining), text);
    gtk_label_set_text(GTK_LABEL(page->remaining_label),
                       left == 1 ? "word left: it must be the secret"
                                 : "words still possible");

    Suggestion top[SUGGESTIONS];
    size_t n = solver_top_answers(&page->solver, top, SUGGESTIONS);
    show_list(page, &page->answers, top, n, 0);
    n = game->status == GAME_PLAYING
        ? solver_top(&page->solver, top, SUGGESTIONS)
        : 0;
    show_list(page, &page->guesses, top, n, 1);

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

static void start(SolvePage *page, const char *secret)
{
    App *app = page->app;
    page->prior = app->prior;
    if (secret != NULL)
        game_start(&page->game, &app->words, secret);
    else
        game_start_random(&page->game, &app->words, &app->priors[page->prior],
                          &app->rng);
    if (page->ready)
        solver_set_prior(&page->solver, app_prior_weights(app, page->prior));
    refresh(page);
}

static void on_new_word(GtkWidget *widget, SolvePage *page)
{
    (void)widget;
    App *app = page->app;
    const char *text = gtk_entry_get_text(GTK_ENTRY(page->entry));
    if (text[0] == '\0')
    {
        start(page, NULL);
        return;
    }
    long index = wordlist_find(&app->words, text);
    if (index < 0)
    {
        gtk_label_set_text(GTK_LABEL(page->status),
                           "That word is not in the word list.");
        return;
    }
    start(page, app->words.items[index]);
    gtk_entry_set_text(GTK_ENTRY(page->entry), "");
}

static void on_probes(GObject *object, GParamSpec *pspec, SolvePage *page)
{
    (void)pspec;
    if (!page->ready)
        return;
    solver_set_probes(&page->solver, gtk_switch_get_active(GTK_SWITCH(object)));
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

static GtkWidget *build_list(RankList *list, const char *title,
                             const char *subtitle, GtkWidget *panel)
{
    GtkWidget *heading = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(heading, 12);
    gtk_container_add(GTK_CONTAINER(heading), label(title, "panel-title"));
    GtkWidget *sub = label(subtitle, "dim");
    gtk_widget_set_valign(sub, GTK_ALIGN_BASELINE);
    gtk_container_add(GTK_CONTAINER(heading), sub);
    gtk_container_add(GTK_CONTAINER(panel), heading);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    for (int i = 0; i < SUGGESTIONS; i++)
    {
        char rank[8];
        g_snprintf(rank, sizeof(rank), "%d.", i + 1);
        list->num[i] = label(rank, "dim");
        gtk_grid_attach(GTK_GRID(grid), list->num[i], 0, i, 1, 1);
        list->word[i] = label("", "rank-word");
        gtk_grid_attach(GTK_GRID(grid), list->word[i], 1, i, 1, 1);
        list->tag[i] = label("probe", "probe-tag");
        gtk_widget_set_tooltip_text(list->tag[i],
                                    "Can't be the secret: played only to "
                                    "split the remaining words");
        gtk_grid_attach(GTK_GRID(grid), list->tag[i], 2, i, 1, 1);
        list->bar[i] = gtk_progress_bar_new();
        if (i == 0)
            style_add(list->bar[i], "best");
        gtk_widget_set_hexpand(list->bar[i], TRUE);
        gtk_widget_set_valign(list->bar[i], GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(grid), list->bar[i], 3, i, 1, 1);
        list->pct[i] = label("", "rank-pct");
        gtk_label_set_xalign(GTK_LABEL(list->pct[i]), 1.0f);
        gtk_label_set_width_chars(GTK_LABEL(list->pct[i]), 6);
        gtk_grid_attach(GTK_GRID(grid), list->pct[i], 4, i, 1, 1);
        // Visibility is driven by show_list, not gtk_widget_show_all.
        GtkWidget *cells[] = { list->num[i], list->word[i], list->tag[i],
                               list->bar[i], list->pct[i] };
        for (size_t c = 0; c < G_N_ELEMENTS(cells); c++)
            gtk_widget_set_no_show_all(cells[c], TRUE);
    }
    gtk_container_add(GTK_CONTAINER(panel), grid);
    return grid;
}

static GtkWidget *build_panel(SolvePage *page)
{
    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    style_add(panel, "panel");
    gtk_widget_set_size_request(panel, 360, -1);
    gtk_widget_set_valign(panel, GTK_ALIGN_START);

    gtk_container_add(GTK_CONTAINER(panel),
                      label("Neural network solver", "panel-title"));

    GtkWidget *secret_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_add(GTK_CONTAINER(secret_row), label("Secret word", "dim"));
    page->secret = label("", "secret");
    gtk_container_add(GTK_CONTAINER(secret_row), page->secret);
    page->secret_kind = label("", "dim");
    gtk_container_add(GTK_CONTAINER(secret_row), page->secret_kind);
    gtk_container_add(GTK_CONTAINER(panel), secret_row);

    GtkWidget *count_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(count_row, 4);
    page->remaining = label("", "panel-big");
    gtk_container_add(GTK_CONTAINER(count_row), page->remaining);
    page->remaining_label = label("", "dim");
    gtk_widget_set_valign(page->remaining_label, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(count_row), page->remaining_label);
    gtk_container_add(GTK_CONTAINER(panel), count_row);

    build_list(&page->answers, "Most likely secret", "exact odds", panel);
    build_list(&page->guesses, "Solver's next guess", "network's choice",
               panel);

    GtkWidget *probe_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(probe_row, 12);
    GtkWidget *probe_label = label("Allow probe guesses", NULL);
    gtk_widget_set_hexpand(probe_label, TRUE);
    gtk_widget_set_tooltip_text(
        probe_row,
        "Let the solver play words that can't be the secret when "
        "they narrow things down better (off: hard mode)");
    gtk_container_add(GTK_CONTAINER(probe_row), probe_label);
    page->probes = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(page->probes), TRUE);
    g_signal_connect(page->probes, "notify::active", G_CALLBACK(on_probes),
                     page);
    gtk_container_add(GTK_CONTAINER(probe_row), page->probes);
    gtk_container_add(GTK_CONTAINER(panel), probe_row);

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(buttons, 8);
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
    gtk_widget_set_margin_top(page->status, 4);
    gtk_container_add(GTK_CONTAINER(panel), page->status);
    return panel;
}

GtkWidget *solve_page_init(SolvePage *page, App *app)
{
    page->app = app;
    page->prior = app->prior;
    page->ready = app->has_model
        && solver_init(&page->solver, &app->words, &app->model,
                       app_prior_weights(app, app->prior))
            == 0;

    page->widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_widget_set_margin_top(page->widget, 18);
    gtk_widget_set_margin_bottom(page->widget, 18);
    gtk_widget_set_margin_start(page->widget, 18);
    gtk_widget_set_margin_end(page->widget, 18);
    gtk_widget_set_halign(page->widget, GTK_ALIGN_CENTER);

    board_init(&page->board, 1);
    gtk_container_add(GTK_CONTAINER(page->widget), page->board.widget);
    gtk_container_add(GTK_CONTAINER(page->widget), build_panel(page));

    start(page, NULL);
    return page->widget;
}

void solve_page_free(SolvePage *page)
{
    if (page->ready)
        solver_free(&page->solver);
    page->ready = 0;
}
