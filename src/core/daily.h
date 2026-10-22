#ifndef DAILY_H
#define DAILY_H

#include <stddef.h>

#include "game.h"
#include "words.h"

// The daily word: one puzzle per local calendar day, the same for everyone
// who has the same word list. Puzzle #1 is on 1 January 2026.
#define DAILY_EPOCH_YEAR 2026
#define DAILY_EPOCH_MONTH 1
#define DAILY_EPOCH_DAY 1

// Daily words are the familiar words (Zipf >= DAILY_ZIPF, not never_secret),
// in an order shuffled once with this fixed seed: no word comes back until
// all of them have been used (about 3,500 days).
#define DAILY_ZIPF 3.0f
#define DAILY_SEED 0x36646c65u // "6dle"

// Puzzle number of a calendar date: 1 on the epoch, 0 or less before it.
long daily_number(int year, int month, int day);

// Puzzle number of today, in local time.
long daily_today(void);

// Seconds until the next puzzle (next local midnight).
long daily_seconds_left(void);

// Index of the word of puzzle `number`, or -1 if no word qualifies (no
// frequencies loaded).
long daily_word(const WordList *words, long number);

// How many different daily words there are before the cycle repeats.
size_t daily_pool_size(const WordList *words);

// Shareable result: "6dle #295 4/7" (or "X/7"), then one line of coloured
// squares per guess. Returns the length written (always NUL-terminated).
size_t daily_share(const Game *game, long number, char *buf, size_t size);

#endif // DAILY_H
