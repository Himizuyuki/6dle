#include "tree.h"
#include <stddef.h>
#include <stdlib.h>

Tree* initList(){
	Tree* tree = malloc(sizeof(Tree));
	tree->letter = 1;
	tree->depth = 0;
	tree->child = malloc(26 * sizeof(Tree));
	for (size_t i = 0; i < 25; i++){

		tree->child[i]->letter = 0;
		tree->child[i]->depth = tree->depth + 1;
		tree->child[i]->child = NULL;
	}
	return tree;
}


// 0 -> error || 1 -> added word
// word -> lowercase 6 letter word
// tree -> the struct tree
int addWord(char* word, Tree* tree){
	if (!tree->letter) {
		return 0; // should be the root of the tree
	}
	Tree* tmp = tree;
	for (size_t i = 0; i < 6; i++) {

		if (tmp->child[(word[i] - 19) % 26]->letter == 0){
			tmp = tmp->child[(word[i] - 19) % 26];
			tmp->letter = 1;
			if (tmp->depth < 6) {
			
				tmp->child = malloc(26 * sizeof(Tree));
				for (size_t j = 0; j < 25; j++){
					
					tmp->child[j]->letter = 0;
					tmp->child[j]->depth = tmp->depth + 1;
					tmp->child[j]->child = NULL;
				}
			}
		}
		else
			tmp = tmp->child[(word[i] - 19) % 26];
	}
	return 1;
}

void freeTreeAux(Tree* tree){

	if (tree == NULL)
		return;
	else{
		for (size_t i = 0; i < 25; i++){

			freeTreeAux(tree->child[i]);
		}
	}
	free(tree->child);
}

void freeTree(Tree* tree){
	
	freeTreeAux(tree);
	free(tree);
}
