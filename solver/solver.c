#include "solver.h"

#include <stdio.h>
#include <stdlib.h>

struct Solver *initSolver(Tree *tree, char nbWords)
{
    struct Solver *solver = malloc(sizeof(struct Solver));
    solver->wordBank = tree;
    if (!solver->wordBank)
    {
        free(solver);
        sprintf(stderr, "Error: could not load word bank from game");
        return NULL;
    }
    solver->nbPreviousWords = 0;
    solver->previousWords = malloc(sizeof(char *) * nbWords);
    solver->wordLength = 6;
    return solver;
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

void chooseRandomWordAux(struct Solver *solver)
{
    Tree *wordBank = solver->wordBank;
    for (size_t i = 0; i < solver->wordLength; i++)
    {
        char arr[26] = {
            0,
        };
        size_t k = 0;
        for (size_t j = 0; j < 26; j++)
        {
            if (wordBank->child[j])
            {
                arr[k++] = j;
            }
        }
        if (k == 0)
        {
            fprintf(stderr, "Error: no word found in word bank\n");
            return;
        }
        char c = arr[randomNumber() % k];
        solver->previousWords[solver->nbPreviousWords][i] = c + 'a';
        wordBank = wordBank->child[c];
    }
}

// choose a word from the wordBank
char *chooseRandomWord(struct Solver *solver)
{
    solver->previousWords[solver->nbPreviousWords] =
        calloc(solver->wordLength + 1, sizeof(char));
    chooseRandomWordAux(solver);
    return solver->previousWords[solver->nbPreviousWords++];
}

void SolverLoop(Game *game)
{
    struct Solver *solver = initSolver(game->WB, maxGuesses);
    if (!solver)
    {
        return;
    }
    // Pure Random Approach lol
    for (size_t i = 0; i < maxGuesses; i++)
    {
        char *word = chooseRandomWord(solver);
    }
}
