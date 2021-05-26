#ifndef RE_H
#define RE_H

#include "sm.h"

/* error codes */
enum {
    RE_ERR_UP = 1, // unbalanced parentheses
    RE_ERR_EX,     // malformed expression
    RE_ERR_STL,    // state transition limit exceeded
    RE_ERR_INIT,   // state machine initialisation failed
    RE_ERR_MEM,    // memory allocation failed in state machine
    RE_OPT = 1     // optimise state machine
};

struct re_matched {
    int start;
    int end;
};

struct sm_fsm*  re_compile(char*, int);
char* re_error_msg(void);
struct re_matched* re_match(struct sm_fsm*, char*);

extern bool debug;
extern int re_error_code;

#endif
