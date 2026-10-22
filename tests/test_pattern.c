#include "core/pattern.h"
#include "core/rng.h"
#include "test.h"

static void check_pattern(const char *guess, const char *secret,
                          const char *expected)
{
    char got[WORD_LEN + 1];
    pattern_to_string(pattern_compute(guess, secret), got);
    test_check(strcmp(got, expected) == 0, __FILE__, __LINE__,
               "pattern(%s, %s) = %s, expected %s", guess, secret, got,
               expected);
}

static void basic_colours(void)
{
    check_pattern("planet", "planet", "gggggg");
    check_pattern("muddly", "planet", "....y.");
    check_pattern("tories", "planet", "y...g.");
    check_pattern("guzzle", "puzzle", ".ggggg");
    check_pattern("aabbcc", "ccaabb", "yyyyyy");
    CHECK_INT(pattern_compute("puzzle", "puzzle"), PATTERN_WIN);
}

static void repeated_letters(void)
{
    // Only as many coloured copies as the secret has.
    check_pattern("aaaaaa", "abcdef", "g.....");
    check_pattern("aaaaaa", "abcdea", "g....g");
    // A green uses up the letter: the earlier copy must not turn yellow.
    check_pattern("axxxxa", "xxxxxa", ".ggggg");
    // Yellows are handed out left to right.
    check_pattern("xeexxx", "eabcdf", ".y....");
    check_pattern("baaaaa", "abbbbb", "yy....");
    // Real words: planet has a single T and a single E, so only one copy
    // of each is coloured.
    check_pattern("teeter", "planet", "y...g.");
}

// Straightforward two-pass implementation with letter counts, to cross-check
// the optimised one.
static Pattern reference(const char *guess, const char *secret)
{
    int count[ALPHABET] = { 0 };
    Mark marks[WORD_LEN];
    for (int i = 0; i < WORD_LEN; i++)
    {
        if (guess[i] == secret[i])
            marks[i] = MARK_CORRECT;
        else
        {
            marks[i] = MARK_ABSENT;
            count[secret[i] - 'a']++;
        }
    }
    for (int i = 0; i < WORD_LEN; i++)
    {
        if (marks[i] != MARK_CORRECT && count[guess[i] - 'a'] > 0)
        {
            count[guess[i] - 'a']--;
            marks[i] = MARK_PRESENT;
        }
    }
    return pattern_from_marks(marks);
}

static void matches_reference_on_random_words(void)
{
    // Small alphabet so repeated letters are frequent.
    Rng rng;
    rng_seed(&rng, 7);
    int mismatches = 0;
    for (int n = 0; n < 200000; n++)
    {
        char g[WORD_LEN + 1] = { 0 };
        char s[WORD_LEN + 1] = { 0 };
        for (int i = 0; i < WORD_LEN; i++)
        {
            g[i] = (char)('a' + rng_below(&rng, 4));
            s[i] = (char)('a' + rng_below(&rng, 4));
        }
        if (pattern_compute(g, s) != reference(g, s))
            mismatches++;
    }
    CHECK_INT(mismatches, 0);
}

static void encoding_round_trips(void)
{
    for (Pattern p = 0; p < PATTERN_COUNT; p++)
    {
        Mark marks[WORD_LEN];
        pattern_to_marks(p, marks);
        CHECK_INT(pattern_from_marks(marks), p);
        char text[WORD_LEN + 1];
        pattern_to_string(p, text);
        Pattern parsed;
        CHECK(pattern_parse(text, &parsed) == 0);
        CHECK_INT(parsed, p);
    }
    Pattern p;
    CHECK(pattern_parse("ggggg", &p) == -1);
    CHECK(pattern_parse("gggggx", &p) == -1);
    CHECK(pattern_parse("ggggggg", &p) == -1);
}

void pattern_tests(void)
{
    RUN(basic_colours);
    RUN(repeated_letters);
    RUN(matches_reference_on_random_words);
    RUN(encoding_round_trips);
}
