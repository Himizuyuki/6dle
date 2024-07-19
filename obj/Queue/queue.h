#ifndef QUEUE_H
#define QUEUE_H

#include "../treeGen/tree.h"
#include <stdlib.h>

struct list
{
    struct list *next;
    struct Tree *data;
};

struct fifo
{
    struct list *head;
    struct list *tail;
    size_t size;
};

struct fifo *fifo_init();
void fifo_push(struct fifo *f, struct Tree *data);
struct Tree *fifo_pop(struct fifo *f);
void fifo_destroy(struct fifo *f);

#endif // QUEUE_H
