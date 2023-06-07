#include "tree.h"
#include <stdlib.h>

int main() {
	// Initialise the tree
	Tree* tree = initTree();
	
	// add Words to the tree
	addWord("colour", tree);
	addWord("pilots", tree);
	
	printTree(tree);
	// remove Words from the tree
	//removeWord("colour", tree);
	//removeWord("probed", tree);
	
	addWord("apples", tree);
	addWord("things", tree);
	addWord("brains", tree);
	addWord("change", tree);
	addWord("cheats", tree);
	
	printTree(tree);

	//removeWord("cheats", tree);

	printTree(tree);
	// Free the struct Tree
	freeTree(tree);

	return EXIT_SUCCESS;
}
