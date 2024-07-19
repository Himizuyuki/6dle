#include "queue.h"

struct fifo *fifo_init()
{
    struct fifo *f = malloc(sizeof(struct fifo));
    if (!f)
    {
        return NULL;
    }
    f->head = NULL;
    f->tail = NULL;
    f->size = 0;
    return f;
}

void fifo_push(struct fifo *f, struct Tree *data)
{
    struct list *l = malloc(sizeof(struct list));
    l->data = data;
    l->next = NULL;
    if (f->size == 0)
    {
        f->head = l;
        f->tail = l;
    }
    else
    {
        f->tail->next = l;
        f->tail = l;
    }
    f->size++;
}

struct Tree *fifo_pop(struct fifo *f)
{
    if (f->size == 0)
        return NULL;
    else if (f->size == 1)
    {
        struct list *l = f->head;
        struct Tree *data = l->data;
        f->head = NULL;
        f->tail = NULL;
        f->size--;
        free(l);
        return data;
    }
    else
    {
        struct list *l = f->head;
        struct Tree *data = l->data;
        f->head = f->head->next;
        f->size--;
        free(l);
        return data;
    }
}

void fifo_clear(struct fifo *f)
{
    while (f->size > 1)
    {
        struct list *l = f->head;
        f->head = f->head->next;
        f->size--;
        free(l);
    }
    if (f->size == 1)
    {
        struct list *l = f->head;
        f->head = NULL;
        f->tail = NULL;
        f->size--;
        free(l);
    }
}

void fifo_destroy(struct fifo *f)
{
    fifo_clear(f);
    free(f);
}
