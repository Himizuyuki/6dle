#include "tree.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

Tree *initTree()
{
    Tree *tree = malloc(sizeof(Tree));
    tree->letter = 0;
    tree->depth = 0;
    tree->child = malloc(26 * sizeof(Tree *));
    for (size_t i = 0; i < 26; i++)
    {
        tree->child[i] = NULL;
    }
    return tree;
}

int addWord(char *word, Tree *tree)
{
    if (tree == NULL)
    {
        return 0; // invalid tree
    }

    Tree *tmp = tree;
    for (size_t i = 0; i < 6; i++)
    {
        int index =
            word[i] - 'a'; // calculate the child index based on the letter

        if (tmp->child[index] == NULL)
        {
            tmp->child[index] =
                initTree(); // create a new child if it doesn't exist
            tmp->child[index]->depth = tmp->depth + 1;
        }

        tmp = tmp->child[index];
    }

    if (tmp->letter == 1)
    {
        return 0; // word already exists
    }

    tmp->letter = 1; // mark the end of the word
    return 1;
}

int removeWordAux(char *word, Tree *tree, int index)
{
    if (tree == NULL)
    {
        return 0; // invalid tree
    }

    if (index == 6)
    {
        free(tree->child);
        if (tree->letter == 1)
        {
            tree->letter = 0; // mark the end of the word as not present
            return 1;
        }
        else
        {
            return 0; // word not found
        }
    }

    int childIndex = word[index] - 'a';
    Tree *child = tree->child[childIndex];
    if (child == NULL)
    {
        return 0; // word not found
    }

    int result = removeWordAux(word, child, index + 1);
    if (result == 1)
    {
        // The word was found in the child subtree and removed
        free(child);
        tree->child[childIndex] = NULL;

        // Check if all children are NULL, if so, free the children array
        int hasChildren = 0;
        for (size_t i = 0; i < 26; i++)
        {
            if (tree->child[i] != NULL)
            {
                hasChildren = 1;
                break;
            }
        }

        if (!hasChildren)
        {
            free(tree->child);
        }

        return 1;
    }

    return 0; // word not found
}

int removeWord(char *word, Tree *tree)
{
    return removeWordAux(word, tree, 0);
}

void freeTreeAux(Tree *tree)
{
    if (tree == NULL)
    {
        return;
    }

    for (size_t i = 0; i < 26; i++)
    {
        freeTreeAux(tree->child[i]);
    }

    free(tree->child);
    free(tree);
}

void freeTree(Tree *tree)
{
    freeTreeAux(tree);
}

void printTreeAux(Tree *tree, char *buffer, int depth)
{
    if (tree == NULL)
    {
        return;
    }

    if (tree->letter == 1)
    {
        buffer[depth] = '\0'; // Null-terminate the buffer to print as a string
        printf("%s\n", buffer);
    }

    for (int i = 0; i < 26; i++)
    {
        if (tree->child[i] != NULL)
        {
            buffer[depth] = 'a' + i; // Add the current character to the buffer
            printTreeAux(tree->child[i], buffer, depth + 1);
        }
    }
}

void printTree(Tree *tree)
{
    char buffer[7]; // Assuming the maximum word length is 6
    printTreeAux(tree, buffer, 0);
    printf("\n");
}
