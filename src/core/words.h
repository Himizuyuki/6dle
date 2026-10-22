#ifndef WORDS_H
#define WORDS_H

#include <stddef.h>

#define WORD_LEN 6
#define ALPHABET 26

// Sorted, de-duplicated list of lowercase six letter words. It is both the
// dictionary of accepted guesses and the pool the secret word is drawn from.
typedef struct WordList
{
    char (*items)[WORD_LEN + 1];
    size_t count;
    float *zipf; // word frequencies (see wordlist_load_frequencies), or NULL
    unsigned char *never_secret; // 1: valid guess, never the secret (or NULL)
} WordList;

// Load a word list, one word per line. Lines are trimmed and lowercased;
// anything that is not exactly WORD_LEN ASCII letters is skipped. The file is
// never modified. Returns 0 on success, -1 if the file can't be read or
// contains no valid word.
int wordlist_load(WordList *list, const char *path);

// Build a list from an array of strings (same filtering as wordlist_load).
int wordlist_from_array(WordList *list, const char *const *words, size_t n);

void wordlist_free(WordList *list);

// Read word frequencies, one "word zipf" pair per line ('#' starts a
// comment), into list->zipf. Zipf is log10 of occurrences per billion words;
// unlisted words get 0. Returns 0 on success, -1 if the file can't be read.
int wordlist_load_frequencies(WordList *list, const char *path);

// Read the words that must never be picked as the secret (slurs, vulgar
// words), one per line, '#' starts a comment, into list->never_secret.
// Returns 0 on success, -1 if the file can't be read.
int wordlist_load_never_secret(WordList *list, const char *path);

// 1 if the word (index) may be picked as the secret.
int word_can_be_secret(const WordList *list, size_t index);

// Index of word in the list, or -1. Accepts upper or lower case.
long wordlist_find(const WordList *list, const char *word);

// 1 if s is exactly WORD_LEN lowercase ASCII letters.
int word_is_valid(const char *s);

#endif // WORDS_H
