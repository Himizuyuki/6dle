#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <criterion/redirect.h>

#include "../Loader/loader.h"
#include "queue.h"

TestSuite(Tests);

Test(Tests, init_Empty_Queue)
{
    struct fifo *q = fifo_init();
    cr_assert_not_null(q, "fifo_init() failed");
    cr_assert_eq(q->head, NULL, "head is not NULL");
    cr_assert_eq(q->tail, NULL, "tail is not NULL");
    cr_assert_eq(q->size, 0, "size is not 0");

    fifo_destroy(q);
}

Test(Tests, enqueue_One_NULL_Element)
{
    struct fifo *q = fifo_init();
    cr_assert_not_null(q, "fifo_init() failed");
    cr_assert_eq(q->head, NULL, "head is not NULL");
    cr_assert_eq(q->tail, NULL, "tail is not NULL");
    cr_assert_eq(q->size, 0, "size is not 0");

    fifo_push(q, NULL);
    cr_assert_eq(q->size, 1, "size is not 1");

    fifo_destroy(q);
}

Test(Tests, enqueue_One_Element)
{
    struct fifo *q = fifo_init();
    cr_assert_not_null(q, "fifo_init() failed");
    cr_assert_eq(q->head, NULL, "head is not NULL");
    cr_assert_eq(q->tail, NULL, "tail is not NULL");
    cr_assert_eq(q->size, 0, "size is not 0");

    Tree *tree = Tloader("SmallSample.txt");
    cr_assert_not_null(tree, "tree is NULL");
    fifo_push(q, tree);

    cr_assert_eq(q->size, 1, "size is not 1");
    cr_assert_not_null(q->head, "head is NULL");
    cr_assert_not_null(q->tail, "tail is NULL");

    fifo_destroy(q);
    freeTree(tree);
}

Test(Tests, enqueue_Multiple_Elements_From_Root_Tree)
{
    struct fifo *q = fifo_init();
    cr_assert_not_null(q, "fifo_init() failed");
    cr_assert_eq(q->head, NULL, "head is not NULL");
    cr_assert_eq(q->tail, NULL, "tail is not NULL");
    cr_assert_eq(q->size, 0, "size is not 0");

    Tree *tree = Tloader("SmallSample.txt");
    cr_assert_not_null(tree, "tree is NULL");
    Tree *subtree = tree->child['i' - 'a'];
    cr_assert_not_null(subtree, "subtree is NULL");
    Tree *subtree2 = tree->child['t' - 'a'];
    cr_assert_not_null(subtree2, "subtree2 is NULL");

    fifo_push(q, tree);
    fifo_push(q, subtree);
    fifo_push(q, subtree2);

    cr_assert_eq(q->size, 3, "size is not 3");
    cr_assert_not_null(q->head, "head is NULL");
    cr_assert_not_null(q->tail, "tail is NULL");

    fifo_destroy(q);
    freeTree(tree);
}

Test(Tests, enqueue_Multiple_Elements_From_SubTree)
{
    struct fifo *q = fifo_init();
    cr_assert_not_null(q, "fifo_init() failed");
    cr_assert_eq(q->head, NULL, "head is not NULL");
    cr_assert_eq(q->tail, NULL, "tail is not NULL");
    cr_assert_eq(q->size, 0, "size is not 0");

    Tree *tree = Tloader("SmallSample.txt");
    cr_assert_not_null(tree, "tree is NULL");
    Tree *subtree = tree->child['i' - 'a'];
    cr_assert_not_null(subtree, "subtree is NULL");
    Tree *subtree2 = subtree->child['n' - 'a'];
    cr_assert_not_null(subtree2, "subtree2 is NULL");

    fifo_push(q, subtree);
    fifo_push(q, subtree2);

    cr_assert_eq(q->size, 2, "size is not 2");
    cr_assert_not_null(q->head, "head is NULL");
    cr_assert_not_null(q->tail, "tail is NULL");

    fifo_destroy(q);
    freeTree(tree);
}

Test(Tests, pop_One_Element)
{
    struct fifo *q = fifo_init();
    cr_assert_not_null(q, "fifo_init() failed");
    cr_assert_eq(q->head, NULL, "head is not NULL");
    cr_assert_eq(q->tail, NULL, "tail is not NULL");
    cr_assert_eq(q->size, 0, "size is not 0");

    Tree *tree = Tloader("SmallSample.txt");
    cr_assert_not_null(tree, "tree is NULL");
    fifo_push(q, tree);

    cr_assert_eq(q->size, 1, "size is not 1");
    cr_assert_not_null(q->head, "head is NULL");
    cr_assert_not_null(q->tail, "tail is NULL");

    Tree *popped = fifo_pop(q);
    cr_assert_eq(q->size, 0, "size is not 0");
    cr_assert_eq(popped, tree,
                 "popped element is not the same as the pushed one");

    fifo_destroy(q);
    freeTree(tree);
}

Test(Tests, pop_Multple_Elements)
{
    struct fifo *q = fifo_init();
    cr_assert_not_null(q, "fifo_init() failed");
    cr_assert_eq(q->head, NULL, "head is not NULL");
    cr_assert_eq(q->tail, NULL, "tail is not NULL");
    cr_assert_eq(q->size, 0, "size is not 0");

    Tree *tree = Tloader("SmallSample.txt");
    cr_assert_not_null(tree, "tree is NULL");
    Tree *subtree = tree->child['i' - 'a'];
    cr_assert_not_null(subtree, "subtree is NULL");
    Tree *subtree2 = tree->child['t' - 'a'];
    cr_assert_not_null(subtree2, "subtree2 is NULL");

    fifo_push(q, tree);
    fifo_push(q, subtree);
    fifo_push(q, subtree2);

    cr_assert_eq(q->size, 3, "size is not 3");
    cr_assert_not_null(q->head, "head is NULL");
    cr_assert_not_null(q->tail, "tail is NULL");

    Tree *popped = fifo_pop(q);
    cr_assert_eq(q->size, 2, "size is not 2");
    cr_assert_eq(popped, tree,
                 "popped element is not the same as the pushed one");

    popped = fifo_pop(q);
    cr_assert_eq(q->size, 1, "size is not 1");
    cr_assert_eq(popped, subtree,
                 "popped element is not the same as the pushed one");

    popped = fifo_pop(q);
    cr_assert_eq(q->size, 0, "size is not 0");
    cr_assert_eq(popped, subtree2,
                 "popped element is not the same as the pushed one");

    fifo_destroy(q);
    freeTree(tree);
}
