#include "style.h"

static const char CSS[] =
    "window.sixdle, window.sixdle stack { background-color: #121213; }\n"
    ".tile {\n"
    "  min-width: 52px; min-height: 52px;\n"
    "  font-size: 26px; font-weight: bold; color: #ffffff;\n"
    "  border: 2px solid #3a3a3c; background-color: transparent;\n"
    "}\n"
    ".tile.filled { border-color: #6e6e70; }\n"
    ".tile.correct { background-color: #538d4e; border-color: #538d4e; }\n"
    ".tile.present { background-color: #b59f3b; border-color: #b59f3b; }\n"
    ".tile.absent { background-color: #3a3a3c; border-color: #3a3a3c; }\n"
    ".tile.invalid { border-color: #e5484d; }\n"
    "button.key {\n"
    "  min-width: 30px; min-height: 46px; padding: 0 6px;\n"
    "  font-weight: bold; color: #ffffff;\n"
    "  background-image: none; background-color: #818384;\n"
    "  border: none; border-radius: 4px; box-shadow: none; text-shadow: "
    "none;\n"
    "}\n"
    "button.key:hover { background-color: #9a9c9e; }\n"
    "button.key.wide { min-width: 62px; font-size: 11px; }\n"
    "button.key.correct { background-color: #538d4e; }\n"
    "button.key.present { background-color: #b59f3b; }\n"
    "button.key.absent { background-color: #3a3a3c; }\n"
    ".note { color: #8e8e93; font-size: 13px; }\n"
    ".status { font-size: 15px; font-weight: bold; color: #ffffff; }\n"
    ".panel {\n"
    "  background-color: #1c1c1e; border-radius: 10px;\n"
    "  padding: 18px;\n"
    "}\n"
    ".panel-title { font-size: 17px; font-weight: bold; color: #ffffff; }\n"
    ".panel-big { font-size: 28px; font-weight: bold; color: #ffffff; }\n"
    ".dim { color: #8e8e93; }\n"
    ".secret { font-family: monospace; font-size: 18px; font-weight: bold;\n"
    "  color: #ffffff; letter-spacing: 3px; }\n"
    ".rank-word { font-family: monospace; font-size: 16px; font-weight: "
    "bold; color: #ffffff; }\n"
    ".rank-pct { font-family: monospace; color: #d1d1d6; }\n"
    "progressbar trough { min-height: 10px; border-radius: 5px;\n"
    "  background-color: #2c2c2e; border: none; }\n"
    "progressbar progress { min-height: 10px; border-radius: 5px;\n"
    "  background-color: #538d4e; border: none; }\n"
    "progressbar.best progress { background-color: #6aaa64; }\n";

void style_install(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

void style_add(GtkWidget *widget, const char *css_class)
{
    gtk_style_context_add_class(gtk_widget_get_style_context(widget),
                                css_class);
}

void style_set_state(GtkWidget *widget, const char *state)
{
    static const char *const STATES[] = { "filled", "correct", "present",
                                          "absent", "invalid" };
    GtkStyleContext *ctx = gtk_widget_get_style_context(widget);
    for (size_t i = 0; i < G_N_ELEMENTS(STATES); i++)
        gtk_style_context_remove_class(ctx, STATES[i]);
    if (state != NULL)
        gtk_style_context_add_class(ctx, state);
}
