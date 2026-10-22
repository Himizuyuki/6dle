#include "app.h"
#include "core/daily.h"
#include "style.h"

// A big number with its caption underneath, in column col of grid.
static GtkWidget *add_number(GtkWidget *grid, int col, const char *caption)
{
    GtkWidget *value = gtk_label_new("0");
    style_add(value, "panel-big");
    gtk_grid_attach(GTK_GRID(grid), value, col, 0, 1, 1);
    GtkWidget *text = gtk_label_new(caption);
    style_add(text, "dim");
    gtk_label_set_justify(GTK_LABEL(text), GTK_JUSTIFY_CENTER);
    gtk_grid_attach(GTK_GRID(grid), text, col, 1, 1, 1);
    return value;
}

static GtkWidget *build_view(StatsView *view)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(box, 8);

    GtkWidget *numbers = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(numbers), 22);
    gtk_grid_set_column_homogeneous(GTK_GRID(numbers), TRUE);
    gtk_widget_set_halign(numbers, GTK_ALIGN_CENTER);
    view->played = add_number(numbers, 0, "Played");
    view->win = add_number(numbers, 1, "Win %");
    view->streak = add_number(numbers, 2, "Current\nstreak");
    view->best = add_number(numbers, 3, "Max\nstreak");
    gtk_container_add(GTK_CONTAINER(box), numbers);

    GtkWidget *heading = gtk_label_new("Guess distribution");
    style_add(heading, "panel-title");
    gtk_label_set_xalign(GTK_LABEL(heading), 0.0f);
    gtk_widget_set_margin_top(heading, 8);
    gtk_container_add(GTK_CONTAINER(box), heading);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    for (int i = 0; i < MAX_GUESSES; i++)
    {
        char n[4];
        g_snprintf(n, sizeof(n), "%d", i + 1);
        GtkWidget *label = gtk_label_new(n);
        style_add(label, "dim");
        gtk_grid_attach(GTK_GRID(grid), label, 0, i, 1, 1);
        view->bar[i] = gtk_progress_bar_new();
        style_add(view->bar[i], "dist");
        gtk_widget_set_size_request(view->bar[i], 240, -1);
        gtk_widget_set_valign(view->bar[i], GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(grid), view->bar[i], 1, i, 1, 1);
        view->count[i] = gtk_label_new("0");
        style_add(view->count[i], "rank-pct");
        gtk_label_set_xalign(GTK_LABEL(view->count[i]), 1.0f);
        gtk_label_set_width_chars(GTK_LABEL(view->count[i]), 4);
        gtk_grid_attach(GTK_GRID(grid), view->count[i], 2, i, 1, 1);
    }
    gtk_container_add(GTK_CONTAINER(box), grid);

    view->lost = gtk_label_new("");
    style_add(view->lost, "dim");
    gtk_label_set_xalign(GTK_LABEL(view->lost), 0.0f);
    gtk_container_add(GTK_CONTAINER(box), view->lost);
    return box;
}

static void set_number(GtkWidget *label, unsigned value)
{
    char text[16];
    g_snprintf(text, sizeof(text), "%u", value);
    gtk_label_set_text(GTK_LABEL(label), text);
}

static void show_view(StatsView *view, const StatsBlock *block, unsigned streak)
{
    set_number(view->played, block->played);
    set_number(view->win, stats_win_percent(block));
    set_number(view->streak, streak);
    set_number(view->best, block->best_streak);
    unsigned max = 1;
    for (int i = 0; i < MAX_GUESSES; i++)
        max = block->dist[i] > max ? block->dist[i] : max;
    for (int i = 0; i < MAX_GUESSES; i++)
    {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(view->bar[i]),
                                      (double)block->dist[i] / max);
        set_number(view->count[i], block->dist[i]);
        // Highlight the row of the last game won.
        GtkStyleContext *ctx = gtk_widget_get_style_context(view->bar[i]);
        if (block->last_result == i + 1)
            gtk_style_context_add_class(ctx, "best");
        else
            gtk_style_context_remove_class(ctx, "best");
    }
    char text[96];
    unsigned lost = block->played - block->won;
    if (block->played == 0)
        g_snprintf(text, sizeof(text), "No game finished yet.");
    else
        g_snprintf(text, sizeof(text), "Lost: %u%s", lost,
                   block->last_result == 0 ? " (including the last one)" : "");
    gtk_label_set_text(GTK_LABEL(view->lost), text);
}

static void refresh(App *app)
{
    show_view(&app->daily_view, &app->stats.daily,
              stats_daily_streak(&app->stats, daily_today()));
    show_view(&app->free_view, &app->stats.free_play,
              app->stats.free_play.streak);
}

static void on_popover_show(GtkWidget *widget, App *app)
{
    (void)widget;
    refresh(app);
}

GtkWidget *stats_button_new(App *app)
{
    GtkWidget *button = gtk_menu_button_new();
    gtk_container_remove(GTK_CONTAINER(button),
                         gtk_bin_get_child(GTK_BIN(button)));
    gtk_container_add(GTK_CONTAINER(button), gtk_label_new("Statistics"));
    gtk_widget_set_can_focus(button, FALSE);

    GtkWidget *popover = gtk_popover_new(button);
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_add_titled(GTK_STACK(stack), build_view(&app->daily_view),
                         "daily", "Daily");
    gtk_stack_add_titled(GTK_STACK(stack), build_view(&app->free_view), "free",
                         "Free play");
    GtkWidget *switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher),
                                 GTK_STACK(stack));
    gtk_widget_set_halign(switcher, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(content), switcher);
    gtk_container_add(GTK_CONTAINER(content), stack);
    if (app->stats_path[0] == '\0')
    {
        GtkWidget *note = gtk_label_new("Statistics are not saved "
                                        "(no home directory).");
        style_add(note, "dim");
        gtk_container_add(GTK_CONTAINER(content), note);
    }
    gtk_widget_show_all(content);
    gtk_container_add(GTK_CONTAINER(popover), content);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
    g_signal_connect(popover, "show", G_CALLBACK(on_popover_show), app);

    app->stats_popover = popover;
    app->stats_stack = stack;
    return button;
}

void app_stats_changed(App *app)
{
    if (app->stats_path[0] != '\0'
        && stats_save(&app->stats, app->stats_path) != 0)
        g_warning("cannot save statistics to %s", app->stats_path);
    if (app->stats_popover != NULL)
        refresh(app);
}

void app_stats_show(App *app, int daily)
{
    gtk_stack_set_visible_child_name(GTK_STACK(app->stats_stack),
                                     daily ? "daily" : "free");
    refresh(app);
    gtk_popover_popup(GTK_POPOVER(app->stats_popover));
}
