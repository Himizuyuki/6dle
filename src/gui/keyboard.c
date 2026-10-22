#include "keyboard.h"

#include "style.h"

static void on_clicked(GtkButton *button, Keyboard *kb)
{
    int key = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "key"));
    kb->handler(kb->user, key);
}

static GtkWidget *make_key(Keyboard *kb, const char *label, int key)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    style_add(button, "key");
    // Keep keyboard focus on the window so typing keeps working.
    gtk_widget_set_can_focus(button, FALSE);
    g_object_set_data(G_OBJECT(button), "key", GINT_TO_POINTER(key));
    g_signal_connect(button, "clicked", G_CALLBACK(on_clicked), kb);
    return button;
}

void keyboard_init(Keyboard *kb, KeyHandler handler, void *user)
{
    static const char *const ROWS[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
    kb->handler = handler;
    kb->user = user;
    kb->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_halign(kb->widget, GTK_ALIGN_CENTER);

    for (size_t r = 0; r < G_N_ELEMENTS(ROWS); r++)
    {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_widget_set_halign(row, GTK_ALIGN_CENTER);
        if (r == 2)
        {
            GtkWidget *enter = make_key(kb, "ENTER", KEY_ENTER);
            style_add(enter, "wide");
            gtk_container_add(GTK_CONTAINER(row), enter);
        }
        for (const char *c = ROWS[r]; *c != '\0'; c++)
        {
            char label[2] = { (char)(*c - 'a' + 'A'), '\0' };
            GtkWidget *key = make_key(kb, label, *c);
            kb->keys[*c - 'a'] = key;
            gtk_container_add(GTK_CONTAINER(row), key);
        }
        if (r == 2)
        {
            GtkWidget *back = make_key(kb, "⌫", KEY_BACKSPACE);
            style_add(back, "wide");
            gtk_container_add(GTK_CONTAINER(row), back);
        }
        gtk_container_add(GTK_CONTAINER(kb->widget), row);
    }
}

void keyboard_show(Keyboard *kb, const KeyState keys[ALPHABET])
{
    static const char *const STATE_CLASS[] = { NULL, "absent", "present",
                                               "correct" };
    for (int l = 0; l < ALPHABET; l++)
        style_set_state(kb->keys[l], STATE_CLASS[keys[l]]);
}
