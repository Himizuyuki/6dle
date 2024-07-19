#ifndef TREE_H
#define TREE_H

//general tree
typedef struct Tree{
    
    char letter; // bool if the letter is present
    char depth; // depth in the tree max of 6
    struct Tree **child; // linked list to children

} Tree;
// for letter char:
//      1 is going to be the root
//      0 is going to be a non existant letter for a given word
//      otherwise number is going to be given to the letter in ascii
//
//  for child*,
//      points towards all the children from 0 to 25 (or the letters a to z)

// Initialise the whole tree
Tree* initTree();

// add a Word to the tree supposing the word is 6 letters long
int addWord(char* word, Tree* tree);

// remove a Word to the tree supposing the word is 6 letters long
int removeWord(char* word, Tree* tree);

int removeChar(char ch, Tree* tree);
int removeCharTotal(char ch, Tree* tree);
int removeCharFromDepth(char ch, Tree* tree, int depth);

// free the whole structure 
void freeTree(Tree* tree);

// print the tree
void printTree(Tree* tree);
#endif // TREE_H
