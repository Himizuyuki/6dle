#ifndef TREE_H
#define TREE_H

//general tree
typedef struct Tree{
    
    char letter; 
    struct Tree **child;

} Tree;
// for letter char:
//      1 is going to be the root
//      0 is going to be a non existant letter for a given word
//      otherwise number is going to be given to the letter in ascii
//
//  for child*,
//      points towards all the children from 0 to 25 (or the letters a to z)

#endif // TREE_H
