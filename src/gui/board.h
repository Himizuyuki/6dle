#ifndef BOARD_H
#define BOARD_H

#include <gtk/gtk.h>

#include "core/game.h"

// The grid of letter tiles, MAX_GUESSES rows of WORD_LEN tiles, with an
// optional note to the right of each row.
typedef struct Board
{
    GtkWidget *widget;
    GtkWidget *tiles[MAX_GUESSES][WORD_LEN];
    GtkWidget *notes[MAX_GUESSES]; // NULL when created without notes
} Board;

void board_init(Board *board, int with_notes);

// Show the guesses of game plus, on the next row, the letters being typed.
void board_show(Board *board, const Game *game, const char *typed);

// Outline the tiles of a row in red (cleared by the next board_show).
void board_flag_row(Board *board, int row);

void board_set_note(Board *board, int row, const char *text);

#endif // BOARD_H
