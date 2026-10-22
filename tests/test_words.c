#include <stdlib.h>
#include <string.h>

#include "core/words.h"
#include "test.h"

static char *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc((size_t)len + 1);
    *size = fread(data, 1, (size_t)len, f);
    fclose(f);
    return data;
}

static void load_filters_sorts_and_dedups(void)
{
    WordList list;
    REQUIRE(wordlist_load(&list, FIXTURE("small.txt")) == 0);
    // 34 well-formed lines, "Planet"/"planet" counted once; junk skipped
    // (5 and 7 letters, apostrophe, digits, two words, empty line).
    CHECK_INT(list.count, 33);
    for (size_t i = 0; i < list.count; i++)
    {
        CHECK(word_is_valid(list.items[i]));
        if (i > 0)
            CHECK(strcmp(list.items[i - 1], list.items[i]) < 0);
    }
    CHECK(wordlist_find(&list, "puzzle") >= 0); // was "  PUZZLE  "
    CHECK(wordlist_find(&list, "sabres") >= 0); // CRLF line
    CHECK(wordlist_find(&list, "meadow") >= 0); // trailing tab
    CHECK(wordlist_find(&list, "apple") < 0);
    CHECK(wordlist_find(&list, "bananas") < 0);
    wordlist_free(&list);
    CHECK(list.items == NULL);
}

static void load_does_not_modify_the_file(void)
{
    // The old loader ran a shell script that rewrote the word list in place.
    size_t before_size;
    size_t after_size;
    char *before = read_file(FIXTURE("small.txt"), &before_size);
    REQUIRE(before != NULL);
    WordList list;
    CHECK(wordlist_load(&list, FIXTURE("small.txt")) == 0);
    wordlist_free(&list);
    char *after = read_file(FIXTURE("small.txt"), &after_size);
    REQUIRE(after != NULL);
    CHECK_INT(before_size, after_size);
    CHECK(memcmp(before, after, before_size) == 0);
    free(before);
    free(after);
}

static void load_reports_errors(void)
{
    WordList list;
    CHECK(wordlist_load(&list, FIXTURE("does-not-exist.txt")) == -1);
    CHECK(list.items == NULL);
    CHECK_INT(list.count, 0);

    const char *junk[] = { "abc", "toolongword", "12345a" };
    CHECK(wordlist_from_array(&list, junk, 3) == -1);
}

static void find_is_case_insensitive_and_strict(void)
{
    const char *words[] = { "zebras", "Apples", "middle" };
    WordList list;
    REQUIRE(wordlist_from_array(&list, words, 3) == 0);
    CHECK_STR(list.items[0], "apples");
    CHECK_INT(wordlist_find(&list, "apples"), 0);
    CHECK_INT(wordlist_find(&list, "APPLES"), 0);
    CHECK_INT(wordlist_find(&list, "zebras"), 2);
    CHECK_INT(wordlist_find(&list, "zebra"), -1);
    CHECK_INT(wordlist_find(&list, "zebrass"), -1);
    CHECK_INT(wordlist_find(&list, ""), -1);
    wordlist_free(&list);
}

static void word_validity(void)
{
    CHECK(word_is_valid("abcdef"));
    CHECK(!word_is_valid("Abcdef"));
    CHECK(!word_is_valid("abcde"));
    CHECK(!word_is_valid("abcdefg"));
    CHECK(!word_is_valid("abc-ef"));
}

static void real_word_bank_loads(void)
{
    WordList list;
    REQUIRE(wordlist_load(&list, SIXDLE_WORDS) == 0);
    CHECK_INT(list.count, 22182);
    CHECK(wordlist_find(&list, "selfie") >= 0); // added to SOWPODS
    CHECK(wordlist_find(&list, "puzzle") >= 0);
    CHECK(wordlist_find(&list, "planet") >= 0);
    wordlist_free(&list);
}

static void frequencies_load(void)
{
    WordList list;
    REQUIRE(wordlist_load(&list, FIXTURE("small.txt")) == 0);
    CHECK(list.zipf == NULL);
    REQUIRE(wordlist_load_frequencies(&list, FIXTURE("small-freq.txt")) == 0);
    REQUIRE(list.zipf != NULL);
    CHECK_NEAR(list.zipf[wordlist_find(&list, "bottle")], 4.3, 1e-6);
    CHECK_NEAR(list.zipf[wordlist_find(&list, "jumble")], 2.8, 1e-6);
    CHECK_NEAR(list.zipf[wordlist_find(&list, "cocoon")], 0.0,
               1e-9); // unlisted
    CHECK(wordlist_load_frequencies(&list, FIXTURE("nope.txt")) == -1);
    CHECK(list.zipf != NULL); // a failed reload keeps the previous values
    wordlist_free(&list);
    CHECK(list.zipf == NULL);
}

static void real_frequencies_cover_the_bank(void)
{
    WordList list;
    REQUIRE(wordlist_load(&list, SIXDLE_WORDS) == 0);
    REQUIRE(wordlist_load_frequencies(&list, SIXDLE_FREQ) == 0);
    size_t known = 0;
    for (size_t i = 0; i < list.count; i++)
        known += list.zipf[i] > 0.0f;
    CHECK(known > 11000); // about half the bank is in the corpus
    CHECK(list.zipf[wordlist_find(&list, "people")] > 6.0f);
    CHECK(list.zipf[wordlist_find(&list, "aaliis")] == 0.0f);
    wordlist_free(&list);
}

void words_tests(void)
{
    RUN(load_filters_sorts_and_dedups);
    RUN(load_does_not_modify_the_file);
    RUN(load_reports_errors);
    RUN(find_is_case_insensitive_and_strict);
    RUN(word_validity);
    RUN(real_word_bank_loads);
    RUN(frequencies_load);
    RUN(real_frequencies_cover_the_bank);
}
