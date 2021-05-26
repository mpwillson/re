/* state machine */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "sm.h"

enum {
    ALLOC_SIZE = 64
};

static struct sm_entry* machine;
static struct sm_fsm* fsm;
static int nstates;
static int max_state;

bool
sm_init(void)
{
    fsm = (struct sm_fsm*) malloc(sizeof(struct sm_fsm));
    max_state = 0;
    nstates = ALLOC_SIZE;
    machine = (struct sm_entry*)
        realloc(NULL, sizeof(struct sm_entry) * nstates);
    return machine != NULL;
}

struct sm_fsm*
sm_get(void)
{
    fsm->fsm = machine;
    fsm->max_state = max_state;
    return fsm;
}

void
sm_set(struct sm_fsm* m)
{
    machine = m->fsm;
    max_state = fsm->max_state;
    return;
}

bool
sm_insert(int state, signed char event, int next1, int next2)
{
    if (state >= nstates) {
        // add more state capacity
        nstates += ALLOC_SIZE;
        machine = (struct sm_entry*) realloc(machine,
                                        sizeof(struct sm_entry) * nstates);
        if (machine == NULL || state > nstates) return false;
    }
    machine[state].event = event;
    machine[state].next1 = next1;
    machine[state].next2 = next2;
    if (state > max_state) max_state = state;
    return true;
}

struct sm_entry*
sm_state(int state)
{
    return (state <= max_state)?machine+state:NULL;

}

void
sm_print(struct sm_fsm* sm)
{
    struct sm_entry* fsm = sm->fsm;

    for (int i=0; i <= sm->max_state; i++) {
        printf("%2d,%c,%2d,%2d\n",i,fsm[i].event,fsm[i].next1,
               fsm[i].next2);
    }
}
