#include "src/game.h"
#include "solver/solver.h"

int main(int argc, char **argv)
{
    argc == 1 ? GameLoop("obj/wordBank.txt") : SolverLoop("obj/wordBank.txt");
}
