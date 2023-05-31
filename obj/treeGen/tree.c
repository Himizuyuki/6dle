#include "tree.h"
#include <stddef.h>
#include <stdlib.h>

Tree* initList(){
	Tree* tree = malloc(sizeof(Tree));
	tree->letter = 1;
	tree->child = malloc(26 * sizeof(Tree));
	size_t n = 97;
	for (size_t i = 0; i < 25; i++){

		tree->child[i]->letter = n + i;
		tree->child[i]->child = NULL;
	}
	return tree;
}



Tree* addWord(char* word, Tree* tree){
	if (!tree->letter) {
		return NULL; // should be the root of the tree
	}
	for (size_t i = 0; i < 6; i++) {
		for (size_t j = 0; j < 25; j++) {
			if (tree->child[j]->letter == word[i]){
				
			}
		}
	}

}
