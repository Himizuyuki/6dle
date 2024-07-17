#include <stdlib.h>

#include "Loader/loader.h"
#include "treeGen/tree.h"

int main()
{
    // int res = loader("../wordBank.txt");
    Tree *tree;
    tree = Tloader("wordBank.txt");
    printTree(tree);
    freeTree(tree);
    return 0;
}
