#include "pattern.h"

#include <stddef.h>

Pattern pattern_compute(const char *guess, const char *secret)
{
    // Secret letters not matched by a green, available for yellows.
    char spare[WORD_LEN];
    size_t nspare = 0;
    unsigned green = 0;
    for (size_t i = 0; i < WORD_LEN; i++)
    {
        if (guess[i] == secret[i])
            green |= 1u << i;
        else
            spare[nspare++] = secret[i];
    }

    Pattern p = 0;
    unsigned weight = 1;
    for (size_t i = 0; i < WORD_LEN; i++, weight *= 3)
    {
        if (green & (1u << i))
        {
            p += MARK_CORRECT * weight;
            continue;
        }
        for (size_t j = 0; j < nspare; j++)
        {
            if (spare[j] == guess[i])
            {
                spare[j] = spare[--nspare];
                p += MARK_PRESENT * weight;
                break;
            }
        }
    }
    return p;
}

void pattern_to_marks(Pattern p, Mark marks[WORD_LEN])
{
    for (size_t i = 0; i < WORD_LEN; i++)
    {
        marks[i] = (Mark)(p % 3);
        p /= 3;
    }
}

Pattern pattern_from_marks(const Mark marks[WORD_LEN])
{
    Pattern p = 0;
    for (size_t i = WORD_LEN; i-- > 0;)
        p = p * 3 + marks[i];
    return p;
}

void pattern_to_string(Pattern p, char *out)
{
    static const char symbols[] = { '.', 'y', 'g' };
    Mark marks[WORD_LEN];
    pattern_to_marks(p, marks);
    for (size_t i = 0; i < WORD_LEN; i++)
        out[i] = symbols[marks[i]];
    out[WORD_LEN] = '\0';
}

int pattern_parse(const char *s, Pattern *out)
{
    Mark marks[WORD_LEN];
    for (size_t i = 0; i < WORD_LEN; i++)
    {
        switch (s[i])
        {
        case '.':
            marks[i] = MARK_ABSENT;
            break;
        case 'y':
            marks[i] = MARK_PRESENT;
            break;
        case 'g':
            marks[i] = MARK_CORRECT;
            break;
        default:
            return -1;
        }
    }
    if (s[WORD_LEN] != '\0')
        return -1;
    *out = pattern_from_marks(marks);
    return 0;
}
