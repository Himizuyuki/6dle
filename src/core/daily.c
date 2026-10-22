#include "daily.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "rng.h"

// Days since 1970-01-01 of a proleptic Gregorian date (Howard Hinnant's
// days_from_civil). Pure arithmetic: no time zone involved.
static long days_from_civil(long y, unsigned m, unsigned d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

long daily_number(int year, int month, int day)
{
    return days_from_civil(year, (unsigned)month, (unsigned)day)
        - days_from_civil(DAILY_EPOCH_YEAR, DAILY_EPOCH_MONTH, DAILY_EPOCH_DAY)
        + 1;
}

long daily_today(void)
{
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    return daily_number(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

long daily_seconds_left(void)
{
    time_t now = time(NULL);
    struct tm midnight;
    localtime_r(&now, &midnight);
    midnight.tm_mday++;
    midnight.tm_hour = 0;
    midnight.tm_min = 0;
    midnight.tm_sec = 0;
    midnight.tm_isdst = -1;
    return (long)difftime(mktime(&midnight), now);
}

static int eligible(const WordList *words, size_t i)
{
    return words->zipf != NULL && words->zipf[i] >= DAILY_ZIPF
        && word_can_be_secret(words, i);
}

size_t daily_pool_size(const WordList *words)
{
    size_t n = 0;
    for (size_t i = 0; i < words->count; i++)
        n += (size_t)eligible(words, i);
    return n;
}

long daily_word(const WordList *words, long number)
{
    size_t n = daily_pool_size(words);
    if (n == 0)
        return -1;
    uint32_t *pool = malloc(n * sizeof(*pool));
    if (pool == NULL)
        return -1;
    size_t k = 0;
    for (size_t i = 0; i < words->count; i++)
    {
        if (eligible(words, i))
            pool[k++] = (uint32_t)i;
    }
    // Fisher-Yates with a fixed seed: the same order on every machine.
    Rng rng;
    rng_seed(&rng, DAILY_SEED);
    for (size_t i = n - 1; i > 0; i--)
    {
        size_t j = rng_below(&rng, i + 1);
        uint32_t tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }
    long slot = (number - 1) % (long)n;
    if (slot < 0)
        slot += (long)n;
    long word = pool[slot];
    free(pool);
    return word;
}

size_t daily_share(const Game *game, long number, char *buf, size_t size)
{
    static const char *const SQUARE[] = { "⬛", "🟨", "🟩" };
    if (size == 0)
        return 0;
    size_t len = 0;
    int n;
    if (game->status == GAME_WON)
        n = snprintf(buf, size, "6dle #%ld %d/%d\n", number, game->nguesses,
                     MAX_GUESSES);
    else
        n = snprintf(buf, size, "6dle #%ld X/%d\n", number, MAX_GUESSES);
    if (n > 0)
        len = (size_t)n < size ? (size_t)n : size - 1;
    for (int r = 0; r < game->nguesses; r++)
    {
        Mark marks[WORD_LEN];
        pattern_to_marks(game->patterns[r], marks);
        for (int c = 0; c <= WORD_LEN && len < size; c++)
        {
            n = snprintf(buf + len, size - len, "%s",
                         c < WORD_LEN ? SQUARE[marks[c]] : "\n");
            if (n > 0)
                len += (size_t)n < size - len ? (size_t)n : size - len - 1;
        }
    }
    return len;
}
