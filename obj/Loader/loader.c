#include "loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <aio.h>
#include <err.h>
#include <ctype.h>
#include "../treeGen/tree.h"
// I need a function that:
//  -loads the Word Bank txt (from a path)
//      .check for the length of the words (see if its possible to remove it after)
//  -adds a word to the Word bank (with or without the tree)
//  -Sort the Word Bank in alphabetical order
//  -remove a word from the word bank (with or without the tree)
Tree* Tloader(const char* path){

    Tree* tree = initTree();
    FILE* f;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    f = fopen(path, "r");
    if (f == NULL)
        errx(EXIT_FAILURE, "EXIT_FAILURE");
    char word[7];
    while (fscanf(f, "%6s", word) == 1) {
        printf("Word :%s | length : %zu\n", line, len);
        if (len != 6) // 6 letter word + \n
            //TODO: Remove Word from txt file
            errx(EXIT_FAILURE, "Not implemented: Word too long");
        else{
            for (size_t i = 0; i < 6; i++){
                word[i] = tolower(line[i]);
            }
            word[6] = 0;
            printf("%s\n", word);
            addWord(word, tree);
        }
    }
    fclose(f);
    if (line)
        free(line);
    return tree;
}


