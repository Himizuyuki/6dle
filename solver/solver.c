#include "solver.h"
#include <stdlib.h>

/* TODO:
 *  Take a random word since "any" word can be a hidden word from the word bank
 *  you go through the whole word bank and remove the possible outcomes
 */

struct Solver* initSolver(char* WBPath, char nbWords)
{
    struct Solver* solver = malloc(sizeof(struct Solver));
    solver->wordBank = Tloader(WBPath);
    solver->nbPreviousWords = 0;
    solver->previousWords = malloc(sizeof(char*) * nbWords);
    solver->wordLength = 6;
    return solver;
}

// choose a word from the wordBank
char* chooseWord(struct Solver* solver)
{
    char* res = calloc(solver->wordLength + 1, sizeof(char));
    solver->previousWords[solver->nbPreviousWords++] = res;
    return res;
}
