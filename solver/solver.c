#include "solver.h"

#include <stdio.h>
#include <stdlib.h>

/* TODO:
 *  Take a random word since "any" word can be a hidden word from the word bank
 *  you go through the whole word bank and remove the possible outcomes
 */

struct Solver *initSolver(char *WB_PATH, char nbWords)
{
    struct Solver *solver = malloc(sizeof(struct Solver));
    solver->wordBank = Tloader(WB_PATH);
    if (!solver->wordBank)
    {
        free(solver);
        sprintf(stderr, "Error: could not load word bank from %s\n", WB_PATH);
        return NULL;
    }
    solver->nbPreviousWords = 0;
    solver->previousWords = malloc(sizeof(char *) * nbWords);
    solver->wordLength = 6;
    return solver;
}

// choose a word from the wordBank
char *chooseWord(struct Solver *solver)
{
    char *res = calloc(solver->wordLength + 1, sizeof(char));
    solver->previousWords[solver->nbPreviousWords++] = res;
    return res;
}

void freeSolver(struct Solver *solver)
{
    for (size_t i = 0; i < solver->nbPreviousWords; i++)
    {
        free(solver->previousWords[i]);
    }
    free(solver->previousWords);
    free(solver);
}
