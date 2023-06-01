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
				for (size_t j = 0; j <= 25; j++){
					
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

int removeWordAux(char* word, Tree* tree, int i){
	
	if (tree->depth == 6)

		if (tree->letter != 1)
			return 0;
		else {
			tree->letter = 0;
			return 1;
		}
	else{
		
		if (tree->child[(word[i] - 19) % 26]->letter == 1){
			
			int res = removeWordAux(word, tree->child[(word[i] - 19) % 26], i+1);
			if (res == 0)
				return 0;
			else{

				tree->letter = 0;
				// go through the children list, if there aren't any letters , free and set to null
				size_t j = 0;
				for (; j <= 25;){
					
					if (tree->child[(word[i] - 19) % 26]->letter == 1)
						break;
					j++;
				}
				if (j == 26){
					free(tree->child);
				}
				return 1;
			}
		}
		else {
			return 0;
		}
	}
}

// assuming the word is a 6 letter word
int removeWord(char* word, Tree* tree){
	return removeWordAux(word, tree, 0);
}

void freeTreeAux(Tree* tree){

	if (tree == NULL)
		return;
	else{
		for (size_t i = 0; i <= 25; i++){

			freeTreeAux(tree->child[i]);
		}
	}
	free(tree->child);
}

void freeTree(Tree* tree){
	
	freeTreeAux(tree);
	free(tree);
}
