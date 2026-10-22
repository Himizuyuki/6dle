#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "style.h"

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, App *app)
{
    (void)widget;
    if (gtk_stack_get_visible_child(GTK_STACK(app->stack)) != app->play.widget
        || (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
        return FALSE;
    guint key = gdk_keyval_to_lower(event->keyval);
    if (key >= GDK_KEY_a && key <= GDK_KEY_z)
        play_page_key(&app->play, 'a' + (int)(key - GDK_KEY_a));
    else if (key == GDK_KEY_Return || key == GDK_KEY_KP_Enter)
        play_page_key(&app->play, KEY_ENTER);
    else if (key == GDK_KEY_BackSpace)
        play_page_key(&app->play, KEY_BACKSPACE);
    else
        return FALSE;
    return TRUE;
}

static void on_activate(GtkApplication *gtk_app, App *app)
{
    if (app->window != NULL)
    {
        gtk_window_present(GTK_WINDOW(app->window));
        return;
    }
    style_install();

    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "6dle");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 900, 780);
    style_add(app->window, "sixdle");

    app->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app->stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_add_titled(GTK_STACK(app->stack), play_page_init(&app->play, app),
                         "play", "Play");
    gtk_stack_add_titled(GTK_STACK(app->stack),
                         solve_page_init(&app->solve, app), "solve", "Solver");

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    GtkWidget *switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher),
                                 GTK_STACK(app->stack));
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(header), switcher);
    gtk_window_set_titlebar(GTK_WINDOW(app->window), header);

    gtk_container_add(GTK_CONTAINER(app->window), app->stack);
    g_signal_connect(app->window, "key-press-event", G_CALLBACK(on_key_press),
                     app);
    gtk_widget_show_all(app->window);
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [--words FILE] [--model FILE]\n"
            "  --words FILE  word list (default %s)\n"
            "  --model FILE  solver network (default %s)\n",
            prog, SIXDLE_WORDS, SIXDLE_MODEL);
}

int main(int argc, char **argv)
{
    const char *words_path = SIXDLE_WORDS;
    App app;
    memset(&app, 0, sizeof(app));
    app.model_path = SIXDLE_MODEL;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--words") == 0 && i + 1 < argc)
            words_path = argv[++i];
        else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc)
            app.model_path = argv[++i];
        else
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    errno = 0;
    if (wordlist_load(&app.words, words_path) != 0)
    {
        fprintf(stderr, "6dle: cannot load word list %s: %s\n", words_path,
                errno ? strerror(errno) : "no six letter words");
        return EXIT_FAILURE;
    }
    app.has_model = mlp_load(&app.model, app.model_path) == 0;
    if (!app.has_model)
        fprintf(stderr, "6dle: no solver model at %s (run `make train`)\n",
                app.model_path);
    rng_seed_random(&app.rng);
    // The board is designed on a dark background; use GTK's built-in dark
    // theme for the rest unless the user picked a theme explicitly.
    if (g_getenv("GTK_THEME") == NULL)
        g_setenv("GTK_THEME", "Adwaita:dark", TRUE);

    GtkApplication *gtk_app = gtk_application_new("io.github.himizuyuki.sixdle",
                                                  G_APPLICATION_NON_UNIQUE);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(on_activate), &app);
    // Our options are parsed above; GTK only sees the program name.
    int status = g_application_run(G_APPLICATION(gtk_app), 1, argv);
    g_object_unref(gtk_app);

    play_page_free(&app.play);
    solve_page_free(&app.solve);
    if (app.has_model)
        mlp_free(&app.model);
    wordlist_free(&app.words);
    return status;
}
