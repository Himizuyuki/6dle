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
        printf("Error: could not load word bank\n");
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

void updateTree(struct Solver *solver, struct SendInfo *info)
{
    Tree *wordBank = solver->wordBank;
    for (size_t i = 0; i < solver->wordLength; i++)
    {
        if (info->colorWords[info->nb_Guesses][i] == 'b')
        {
            removeChar(solver->previousWords[info->nb_Guesses][i], wordBank);
        }
        else if (info->colorWords[info->nb_Guesses][i] == 'y')
        {
            removeCharFromDepth(solver->previousWords[info->nb_Guesses][i], wordBank, i);
        }
        else if (info->colorWords[info->nb_Guesses][i] == 'g')
        {
            for (size_t j = 0; j < 26; j++)
            {
                if (wordBank->child[j] && j != info->colorWords[info->nb_Guesses][i] - 'a')
                {
                    removeCharFromDepth(j + 'a', wordBank, i);
                }
            }
        }
    }
}

void UpdateInfo(Game *game, struct SendInfo *info)
{
    info->nb_Guesses = game->nb_Guesses;
    for (size_t j = 0; j < 6; j++)
    {
        info->colorWords[game->nb_Guesses][j] = game->colorWords[game->nb_Guesses][j];
    }
}

void SolverLoop(char* WBpath)
{
    Game *game = initGame(WBpath);
    struct Solver *solver = initSolver(game->WB, maxGuesses);
    if (!solver)
    {
        return;
    }
    struct SendInfo *info = calloc(1, sizeof(struct SendInfo));
    while(game->nb_Guesses < maxGuesses)
    {
        char *word = chooseRandomWord(solver);
        printf("Guessing word %s\n", word);
        for (size_t i = 0; i < solver->wordLength; i++)
        {
            game->guessedWords[game->nb_Guesses][i] = solver->previousWords[game->nb_Guesses][i];
        }
        colorWord(game);
        UpdateInfo(game, info);
        updateTree(solver, info);
        game->nb_Guesses++;
        if (game->found)
            break;
    }
    if (game->found)
    {
        printf("Found the word %s in %d guesses\n",game->Hword, game->nb_Guesses);
    }
    else
    {
        printf("Could not find the word %s in %d guesses\n",game->Hword, game->nb_Guesses);
    }
    free(info);
    freeSolver(solver);
    freeGame(game);
}
