#ifndef SM_H
#define SM_H

struct sm_entry {
    signed char event;
    char *cc;
    int next1;
    int next2;
};

struct sm_fsm {
    struct sm_entry* fsm;
    int max_state;
};


bool sm_init(void);
struct sm_fsm* sm_get(void);
void sm_set(struct sm_fsm*);
bool sm_insert(int, signed char, int, int);
struct sm_entry* sm_state(int);
void sm_print(struct sm_fsm*);

#endif
