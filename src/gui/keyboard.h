#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <gtk/gtk.h>

#include "core/game.h"

#define KEY_ENTER '\n'
#define KEY_BACKSPACE '\b'

// Called with 'a'..'z', KEY_ENTER or KEY_BACKSPACE.
typedef void (*KeyHandler)(void *user, int key);

// On-screen QWERTY keyboard whose keys take the colour of what is known
// about each letter.
typedef struct Keyboard
{
    GtkWidget *widget;
    GtkWidget *keys[ALPHABET];
    KeyHandler handler;
    void *user;
} Keyboard;

void keyboard_init(Keyboard *kb, KeyHandler handler, void *user);

void keyboard_show(Keyboard *kb, const KeyState keys[ALPHABET]);

#endif // KEYBOARD_H
