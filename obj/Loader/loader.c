#include "loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <aio.h>
#include <err.h>
#include <ctype.h>
#include <string.h>
#include "../treeGen/tree.h"
// I need a function that:
//  -loads the Word Bank txt (from a path)
//      .check for the length of the words (see if its possible to remove it after)
//  -adds a word to the Word bank (with or without the tree)
//  -Sort the Word Bank in alphabetical order
//  -remove a word from the word bank (with or without the tree)
Tree* Tloader(const char* path) {
    // Call the shell script to remove invalid words
    char command[100];
    sprintf(command, "./remove_invalid_words.sh %s", path);
    // This shit is so fucking exploitable
    system(command);

    Tree* tree = initTree();
    FILE* f = fopen(path, "r+");
    if (f == NULL) {
        errx(EXIT_FAILURE, "Failed to open file");
    }
    char word[7]; // Adjusted to accommodate 12-letter words
    //long int filePos = ftell(f);
    while (fscanf(f, "%6s", word) == 1) { // Updated to read up to 12 characters
        size_t len = strlen(word);
        for (size_t i = 0; i < len; i++) {
            word[i] = tolower(word[i]);
        }
        addWord(word, tree);
    }
    fclose(f);
    return tree;
}

