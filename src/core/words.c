#include "words.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Builder
{
    char (*items)[WORD_LEN + 1];
    size_t count;
    size_t capacity;
} Builder;

// Normalise one line into out (lowercase, trimmed). Returns 1 if it is a
// valid word.
static int normalise(const char *line, char out[WORD_LEN + 1])
{
    while (isspace((unsigned char)*line))
        line++;
    size_t len = 0;
    while (line[len] != '\0' && !isspace((unsigned char)line[len]))
    {
        if (len == WORD_LEN || !isalpha((unsigned char)line[len]))
            return 0;
        out[len] = (char)tolower((unsigned char)line[len]);
        len++;
    }
    for (size_t i = len; line[i] != '\0'; i++)
    {
        if (!isspace((unsigned char)line[i]))
            return 0;
    }
    out[len] = '\0';
    return len == WORD_LEN;
}

static int builder_push(Builder *b, const char *line)
{
    char word[WORD_LEN + 1];
    if (!normalise(line, word))
        return 0;
    if (b->count == b->capacity)
    {
        size_t capacity = b->capacity ? b->capacity * 2 : 1024;
        void *items = realloc(b->items, capacity * sizeof(*b->items));
        if (items == NULL)
            return -1;
        b->items = items;
        b->capacity = capacity;
    }
    memcpy(b->items[b->count++], word, WORD_LEN + 1);
    return 0;
}

static int compare_words(const void *a, const void *b)
{
    return strcmp(a, b);
}

static int builder_finish(Builder *b, WordList *list)
{
    if (b->count == 0)
    {
        free(b->items);
        return -1;
    }
    qsort(b->items, b->count, sizeof(*b->items), compare_words);
    size_t unique = 1;
    for (size_t i = 1; i < b->count; i++)
    {
        if (strcmp(b->items[i], b->items[unique - 1]) != 0)
            memcpy(b->items[unique++], b->items[i], WORD_LEN + 1);
    }
    list->items = b->items;
    list->count = unique;
    list->zipf = NULL;
    return 0;
}

int wordlist_load(WordList *list, const char *path)
{
    list->items = NULL;
    list->count = 0;
    list->zipf = NULL;
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return -1;

    Builder b = { 0 };
    char *line = NULL;
    size_t cap = 0;
    int err = 0;
    while (!err && getline(&line, &cap, f) != -1)
        err = builder_push(&b, line);
    free(line);
    fclose(f);
    if (err)
    {
        free(b.items);
        return -1;
    }
    return builder_finish(&b, list);
}

int wordlist_from_array(WordList *list, const char *const *words, size_t n)
{
    list->items = NULL;
    list->count = 0;
    list->zipf = NULL;
    Builder b = { 0 };
    for (size_t i = 0; i < n; i++)
    {
        if (builder_push(&b, words[i]) != 0)
        {
            free(b.items);
            return -1;
        }
    }
    return builder_finish(&b, list);
}

void wordlist_free(WordList *list)
{
    free(list->items);
    free(list->zipf);
    list->items = NULL;
    list->zipf = NULL;
    list->count = 0;
}

int wordlist_load_frequencies(WordList *list, const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return -1;
    float *zipf = calloc(list->count, sizeof(*zipf));
    if (zipf == NULL)
    {
        fclose(f);
        return -1;
    }
    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, f) != -1)
    {
        char word[32];
        float value;
        if (line[0] == '#' || sscanf(line, "%31s %f", word, &value) != 2)
            continue;
        long index = wordlist_find(list, word);
        if (index >= 0 && value > 0.0f)
            zipf[index] = value;
    }
    free(line);
    fclose(f);
    free(list->zipf);
    list->zipf = zipf;
    return 0;
}

long wordlist_find(const WordList *list, const char *word)
{
    char key[WORD_LEN + 1];
    if (!normalise(word, key))
        return -1;
    char(*found)[WORD_LEN + 1] = bsearch(key, list->items, list->count,
                                         sizeof(*list->items), compare_words);
    return found ? (long)(found - list->items) : -1;
}

int word_is_valid(const char *s)
{
    for (size_t i = 0; i < WORD_LEN; i++)
    {
        if (s[i] < 'a' || s[i] > 'z')
            return 0;
    }
    return s[WORD_LEN] == '\0';
}
