#include "board.h"

#include <ctype.h>

#include "style.h"

void board_init(Board *board, int with_notes)
{
    board->widget = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(board->widget), 6);
    gtk_grid_set_column_spacing(GTK_GRID(board->widget), 6);
    gtk_widget_set_halign(board->widget, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(board->widget, GTK_ALIGN_START);
    for (int r = 0; r < MAX_GUESSES; r++)
    {
        for (int c = 0; c < WORD_LEN; c++)
        {
            GtkWidget *tile = gtk_label_new("");
            style_add(tile, "tile");
            gtk_grid_attach(GTK_GRID(board->widget), tile, c, r, 1, 1);
            board->tiles[r][c] = tile;
        }
        board->notes[r] = NULL;
        if (with_notes)
        {
            GtkWidget *note = gtk_label_new("");
            style_add(note, "note");
            gtk_label_set_xalign(GTK_LABEL(note), 0.0f);
            gtk_label_set_width_chars(GTK_LABEL(note), 14);
            gtk_widget_set_margin_start(note, 8);
            gtk_grid_attach(GTK_GRID(board->widget), note, WORD_LEN, r, 1, 1);
            board->notes[r] = note;
        }
    }
}

static void set_tile(GtkWidget *tile, char letter, const char *state)
{
    char text[2] = { (char)toupper((unsigned char)letter), '\0' };
    gtk_label_set_text(GTK_LABEL(tile), text);
    style_set_state(tile, state);
}

void board_show(Board *board, const Game *game, const char *typed)
{
    static const char *const MARK_CLASS[] = { "absent", "present", "correct" };
    for (int r = 0; r < MAX_GUESSES; r++)
    {
        if (r < game->nguesses)
        {
            Mark marks[WORD_LEN];
            pattern_to_marks(game->patterns[r], marks);
            for (int c = 0; c < WORD_LEN; c++)
                set_tile(board->tiles[r][c], game->guesses[r][c],
                         MARK_CLASS[marks[c]]);
            continue;
        }
        int typing = typed != NULL && r == game->nguesses
            && game->status == GAME_PLAYING;
        int ntyped = 0;
        for (int c = 0; c < WORD_LEN; c++)
        {
            if (typing && typed[ntyped] != '\0')
                set_tile(board->tiles[r][c], typed[ntyped++], "filled");
            else
                set_tile(board->tiles[r][c], '\0', NULL);
        }
    }
}

void board_flag_row(Board *board, int row)
{
    if (row < 0 || row >= MAX_GUESSES)
        return;
    for (int c = 0; c < WORD_LEN; c++)
        style_add(board->tiles[row][c], "invalid");
}

void board_set_note(Board *board, int row, const char *text)
{
    if (board->notes[row] != NULL)
        gtk_label_set_text(GTK_LABEL(board->notes[row]), text);
}
