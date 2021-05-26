 /*
 * Regular Expressions
 *
 * Grammer:
 * <expression> ::= <term> | <term> '|' <expression>
 * <term>       ::= <factor> | <factor> <term>
 * <factor>     ::= '('<expression>')' | v | <factor>'*'
 *
 * Version 19 - (based on 18)
 * Add . support
 * Fix anchors when used inside alternate expressions
 * 2021-04-07 Passes simple tests
 *
 * Version 20
 * Multiple state machines
 * Error checking for state insertion
 * Added DEBUG/DEBUGV macros
 *
 * Version 21
 * Fixed failure for regexp (a*b|ac)d, caused by killing the backtrack
 * position when handling a closure.
 *
 * Version 22
 * I was building on foundations of sand.  Rewrite as suggested by
 * Sedgewick.
 * This has been more difficult than I thought. There are defects in
 * the implementations in the book (carried forward to the version in
 * C). Issues exist in both the compiler and matcher (of course, this
 * possibility remains my coding is at fault!).
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <setjmp.h>
#ifdef linux
#include <bsd/string.h>
#endif

#include "re.h"
#include "sm.h"
#include "dq.h"

/* DEBUG macro for upto three integers */
#define DEBUG(intro,a,b,c)                      \
    if (debug) fprintf(stderr,"%s: "#a": %2d, "#b": %2d, "#c": %2d\n", \
                       (intro), (a), (b), (c));                        \

/* DEBUG macro for printing varying number of args */
#define DEBUGV(format, ...) \
    if (debug) fprintf(stderr,format, __VA_ARGS__); \

/* special tokens and constant limits
 * assumes char is signed */
enum {
    RE_LP = -1,
    RE_RP = -2,
    RE_OR = -3,
    RE_CL = -4,
    RE_BOL = -5,
    RE_EOL = -6,
    RE_DOT = -7,
    LEXBUFSIZE = 81,
    MAX_TRANSITIONS = 1000
};

char* error_msg[] = {
    "no such error message",
    "unbalanced parentheses",
    "malformed expression",
    "state transition limit exceeded",
    "failed to initialise state machine",
    "unable to allocate memory for state machine"
};

bool debug = false;

/* signed char is implementation dependent */
static char lexbuf[LEXBUFSIZE];
static char* lexnext;

/* forward decls */
static int term(void);
static int factor(void);
static int expression(void);

static jmp_buf env;

// holds code for last error
// declared extern in header for use by client
int re_error_code;

char*
re_error_msg(void)
{
    return error_msg[re_error_code];
}

static void
error(int error_code)
{
    re_error_code = error_code;
    longjmp(env, error_code);
    return;
}

static size_t
lexbuf_init(char* s)
{
    lexnext = lexbuf;
    return strlcpy(lexbuf,s,LEXBUFSIZE);
}

static void
unlexch(void)
{
    if (lexnext > lexbuf) {
        lexnext--;
        if (*(lexnext-1) == '\\') lexnext--;
    }
}

static char
lexch(void)
{
    static bool started = false;
    char c = '\0';

    if (lexnext < lexbuf+LEXBUFSIZE) {
        c = *lexnext++;
        switch (c) {
            case '(':
                c = RE_LP;
                break;
            case ')':
                c = RE_RP;
                break;
            case '|':
                c = RE_OR;
                started = false;
                break;
            case '*':
                c = RE_CL;
                break;
            case '\\':
                c = *lexnext++;
                break;
            case '^':
                c = started?c:RE_BOL;
                break;
            case '$':
                c = (*lexnext == '\0' || *lexnext == ')' ||
                     *lexnext == '|')?RE_EOL:c;
                break;
            case '.':
                c = RE_DOT;
                break;
            default:
                // plain character
                started = true;
                break;
        }
    }
    return c;
}

/* Next available state */
static int state;

int
expression(void)
{
    int t1, t2, expr;
    char c;

    t1 = term();
    expr = t1;
    c = lexch();
    if (c == RE_OR) {
        expr = t2 = ++state;
        state++;
        insert_state(t2,'\0',expression(),t1);
        insert_state(t2-1,'\0',state,state);
    }
    else {
        unlexch();
    }
    return expr;
}

int
term(void)
{
    int t;
    char c;

    t = factor();
    c = lexch(); unlexch();
    if (c > '\0' || c == RE_LP) {
        int t2 = term();
        DEBUG("term", t2, t, state);
    }
    return t;
}

int
factor(void)
{
    int t1, t2, fstate;
    char c;
    struct s_entry* st;

    t1 = state;
    c = lexch();
    if (c == RE_LP) {
        t2 = expression();
        c = lexch();
        DEBUGV("factor: c: %2d/'%c'\n", c, c);
        if (c != RE_RP) error(RE_ERR_UP);
        // Original algorithm produces a machine with unreachable
        // states, e.g. for patterns like "z(a|b)z" where the b
        // alternate was ignored.
        // this is an attempt to fix. Use state from t2 (expression)
        // when t1-1 state is a character or a closure.
        st = get_state(t1-1);
        if (st->event > '\0')
            st->next1 = t2;
        else if (st->next1 != st->next2)
            st->next1 = t2;
    }
    else if (c > '\0') {
        insert_state(state,c,state+1,0);
        t2 = state;
        state++;
    }
    else {
        error(RE_ERR_EX);
    }
    c = lexch();
    if (c != RE_CL) {
        fstate = t2;
        unlexch();
    }
    else {
        insert_state(state, '\0', state+1, t2);
        fstate = state;
        DEBUG("factor: cl", t1, t2, state);
        get_state(t1-1)->next1 = state;
        state++;
    }
    return fstate;
}

struct s_entry*
re_compile(char* re_str, int flags)
{
    int error_code;

    if ((error_code = setjmp(env)) == 0) {
        if (!init_machine()) {
            re_error_code = RE_ERR_INIT;
            return NULL;
        }
        lexbuf_init(re_str);
        state = 1;
        insert_state(0,'\0',expression(),0);
        insert_state(state,'\0', 0, 0);
    }
    return (error_code == 0)?get_machine():NULL;
}

enum {
    SCAN = -1
};

static int
matcher(struct s_entry* machine, char* search_str, int start)
{
    int n1, n2, ntransitions = 0;
    int j = start, N = strlen(search_str), state = machine->next1;
    struct s_entry* st;

    dq_init(30);
    dq_push_tail(SCAN);
    DEBUGV("matcher: searching: %s\n", search_str+j);
    DEBUG("initial", start, N, 0);
    while (state) {
        if (ntransitions++ > MAX_TRANSITIONS) error(RE_ERR_STL);
        st = machine+state;
        DEBUG("---> state", state, j, 0);
        if (debug) dq_print();
        if (state == SCAN) {
            j++;
            dq_push_tail(SCAN);
        }
        else if (st->event == search_str[j] || st->event == RE_DOT) {
            // for reasons unclear, closures within closures causes
            // duplicate pending states to be enqueued (e.g (aa*)*).
            // This test stops that happening
            if (dq_peek_tail() != st->next1) dq_push_tail(st->next1);
        }
        else if (st->event == '\0') {
            n1 = st->next1;
            n2 = st->next2;
            dq_push_head(n1);
            if (n1 != n2) dq_push_head(n2);
        }
        DEBUG("matcher end", dq_empty(), j, N);
        if (debug) dq_print();
        if (dq_empty() || j == N) return -1;
        state = dq_pop_head();
        // attempt to fix a defect in the matcher when
        // closure is last pattern
        if (state == 0 && dq_peek_tail() != SCAN) {
           state = dq_pop_head();
        }
        DEBUG("final", state, 0, 0);
    }
    DEBUG("matcher return", j, 0, 0);
    return j;
}

static int
matcher_mw(struct s_entry* machine, char* search_str, int start)
{
    char c;
    int i = start, tmp = -1, state = 0, back_track = -1,
        ntransitions = 0;
    struct s_entry* st;
    bool endstr = false;

    dq_init(30);
    st = machine;
    if (debug) dq_print();
    dq_push_tail(-1); // end of states
    while (state != -1) {
        ntransitions++;
        if (ntransitions > MAX_TRANSITIONS) {
            re_error_code = RE_ERR_STL;
            return start;
        }

        DEBUG("matcher", state, i, back_track);
        if (debug) dq_print();
        c = search_str[i];
        endstr = (c == '\0');
        if (st->event == '\0') {
            if (st->next1 == 0 && st->next2 == 0) {
                state = -2;
                break;
            }
            if (st->next2 != 0) {
                if (dq_peek_head() != st->next1) {
                    if ((machine+state-1)->next1 == (machine+state-1)->next2) {
                        dq_push_head(i);
                    }
                    else {
                        dq_push_head(-1);
                    }
                    dq_push_head(st->next1);
                    state = st->next2;
                }
            }
            else {
                state = st->next1;
            }
        }
        else {
            if (st->event == c){
                state = st->next1;
                ++i;
            }
            else {
                DEBUG("match fail", state, i, 0);
                if (debug) dq_print();
                int new_state = dq_pop_head();
                // backtrack if at end of OR
                if ((tmp = dq_pop_head()) != -1) i = tmp;
                state = new_state;
            }
        }
        DEBUG("new state", state, 0, 0);
        st = machine+state;
    }
    return (state == -2)?i:start;
}

struct re_matched*
re_match(struct s_entry* machine, char* search_str)
{
    int end;
    static struct re_matched matched;

    for (int i = 0; search_str[i] != '\0'; i++){
        end = matcher(machine,search_str,i);
        if ( end != -1) {//(end > i) {
            matched.start = i;
            matched.end = end;
            return &matched;
        }
    }
    return NULL;
}
