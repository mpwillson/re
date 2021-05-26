/* Circular deque */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "dq.h"

static int* dq = NULL;
static int dqsize, head, tail;

int
dq_init(int size)
{
    if (dq) free(dq);
    dq = malloc(size * sizeof(int));
    dqsize = size;
    head = 1; tail = 1;
    return dq != NULL;
}

int
dq_push_head(int item)
{
    dq[head] = item;
    head = (head-1+dqsize)%dqsize;
    return head;
}

int
dq_push_tail(int item)
{
    tail = (tail+1)%dqsize;
    dq[tail] = item;
    return tail;
}

int
dq_pop_head(void)
{
    head = (head+1+dqsize)%dqsize;
    return dq[head];
}

int
dq_pop_tail(void)
{
    int t = dq[tail];

    tail = (tail-1)%dqsize;
    return t;
}

int
dq_peek_head(void)
{
    return dq[abs(head+1)%dqsize];
}

int
dq_peek_tail(void)
{
    return dq[abs(tail)%dqsize];
}

int
dq_empty(void)
{
    return (tail == head);
}

void
dq_print(void)
{
    fprintf(stderr,"dq: head: %2d, tail: %2d, length: %3d\n", head, tail,
            (tail>head)?(tail-head):(dqsize-(head-tail)));
    for (int i = (head+1)%dqsize; i != (tail+1)%dqsize; i = (i+1)%dqsize)
        fprintf(stderr,"%2d, ", dq[i]);
    fprintf(stderr,"\n");
    return;
}
