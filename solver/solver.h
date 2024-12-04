#ifndef SOLVER_H
#define SOLVER_H

#include "../obj/random.h"
#include "../obj/Loader/loader.h"
#include "../obj/treeGen/tree.h"
#include "../src/game.h"

#include <stdlib.h>
#include <stddef.h>

struct Solver
{
    Tree* wordBank;
    char** previousWords;
    size_t nbPreviousWords;
    size_t wordLength;
};

struct Solver *initSolver(Tree *tree, char nbWords);
void freeSolver(struct Solver *solver);
void SolverLoop(char* WBpath);

#endif // DEBUG
