#ifndef STYLE_H
#define STYLE_H

#include <gtk/gtk.h>

// Load the application stylesheet.
void style_install(void);

void style_add(GtkWidget *widget, const char *css_class);

// Replace the tile/key state class (filled, correct, present, absent,
// invalid) of a widget; state may be NULL.
void style_set_state(GtkWidget *widget, const char *state);

#endif // STYLE_H
