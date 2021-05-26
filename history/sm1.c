/* state machine */
#include <stdio.h>
#include <stdbool.h>

#include "sm.h"

enum {
    NSTATES = 64
};

struct s_entry states[NSTATES];

bool
insert_state(int state,char event,int next1,int next2)
{
    if (state > NSTATES) return false;
    states[state].event = event;
    states[state].next1 = next1;
    states[state].next2 = next2;
    return true;
}

struct s_entry*
get_state(int state)
{
    return states+state;
}

bool
upd_state(int state, int next1, int next2)
{
    if (state > NSTATES) return false;
    states[state].next1 = next1;
    states[state].next2 = next2;
    return true;
}

void
print_states(void)
{
    for (int i=0; i <= NSTATES; i++) {
        printf("%2d,%c,%2d,%2d\n",i,states[i].event,states[i].next1,
               states[i].next2);
        if (states[i].next1 == 0 && states[i].next2 == 0) break;
    }
}
