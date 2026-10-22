#ifndef PATTERN_H
#define PATTERN_H

#include <stdint.h>

#include "words.h"

// Colour of one tile of a guess.
typedef enum Mark
{
    MARK_ABSENT = 0, // grey: letter not in the word (or no copies left)
    MARK_PRESENT = 1, // yellow: letter in the word, elsewhere
    MARK_CORRECT = 2, // green: right letter, right place
} Mark;

// The feedback for a whole guess, packed in base 3: tile i is digit i.
typedef uint16_t Pattern;

#define PATTERN_COUNT 729 // 3^WORD_LEN
#define PATTERN_WIN 728 // every tile green

// Feedback for guess against secret, Wordle rules: greens first, then
// yellows left to right, each secret letter used at most once.
Pattern pattern_compute(const char *guess, const char *secret);

void pattern_to_marks(Pattern p, Mark marks[WORD_LEN]);

Pattern pattern_from_marks(const Mark marks[WORD_LEN]);

// Text form, one char per tile: 'g' green, 'y' yellow, '.' grey.
// out must hold WORD_LEN + 1 chars.
void pattern_to_string(Pattern p, char *out);

// Inverse of pattern_to_string. Returns -1 on malformed input.
int pattern_parse(const char *s, Pattern *out);

#endif // PATTERN_H
