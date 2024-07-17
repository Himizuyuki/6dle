#include "solver.h"

/* TODO:
 *  Take a random word since "any" word can be a hidden word from the word bank
 *  you go through the whole word bank and remove the possible outcomes
 */

int delContainsAux(char c, Tree *wb)
{
    return 0;
}

void delContains(char c, Tree *wb)
{
    delContainsAux(c, wb);
}

// Find a random Word withing the word bank
void findRandom(char *word, Tree *wb)
{
    unsigned int randi;
    word[6] = 0;
    for (size_t i = 0; i < 6; i++)
    {
        char m[26] = {
            0,
        };
        char nbW = 0;
        for (char j = 0; j < 26; j++)
        {
            if (wb->child[j] != NULL)
            {
                m[nbW] = j;
                nbW++;
            }
        }
        randi = randomNumber() % nbW;
        word[i] = m[randi] + 'a';
        wb = wb->child[m[randi]];
    }
}
