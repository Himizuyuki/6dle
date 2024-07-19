#include "tree.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    // Initialise the tree
    Tree *tree = initTree();
    // int r;
    //  add Words to the tree
    addWord("colour", tree);
    addWord("pilots", tree);

    printTree(tree);
    // remove Words from the tree
    /*r = removeWord("colour", tree);
    if (r) printf("colour removed successfully\n");
    r = removeWord("probed", tree);
    if (!r) printf("word was not removed / wasn't in the tree\n");*/

    addWord("apples", tree);
    addWord("things", tree);
    addWord("brains", tree);
    addWord("change", tree);
    addWord("cheats", tree);

    printTree(tree);

    removeChar('c', tree);

    printTree(tree);

    removeCharFromDepth('t', tree, 5);

    printTree(tree);

    // Free the struct Tree
    freeTree(tree);

    return 0;
}
