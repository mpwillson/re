
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#ifdef linux
#include <bsd/string.h>
#endif
#include "sm.h"

/* assumes char is signed */
enum {
    LEXBUFSIZE = 81,
    RE_LP = -1,
    RE_RP = -2,
    RE_OR = -3,
    RE_CL = -4
};

char* badparen = "mismatched parenthesis";

/* signed char is implementation dependent */
static char lexbuf[LEXBUFSIZE];
static char* lexnext;

/* forward decls */
void term(void);
void factor(void);
void expr(void);

void
error(char* msg)
{
    fprintf(stderr,"re: %s\n",msg);
    exit(EXIT_FAILURE);
}

size_t
lexbuf_init(char* s)
{
    lexnext = lexbuf;
    return strlcpy(lexbuf,s,LEXBUFSIZE);
}

void
unlexch(void)
{
    if (lexnext > lexbuf) {
        lexnext--;
        if (*(lexnext-1) == '\\') lexnext--;
    }
}

char
lexch(void)
{
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
                break;
            case '*':
                c = RE_CL;
                break;
            case '\\':
                c = *lexnext++;
                break;
            default:
                c = c;
        }
    }
    return c;
}

int last_expr = 0;
int end_state = 0;
int fail_state = 0;
int closure_fail_state = 0;

int rp_pending = 0;
bool or_pending = false;

void
expr(void)
{
    int expr_start, this_fail_state, complete_state;
    char c;
    struct s_entry* st;

    expr_start = end_state;
    fail_state = expr_start+1;
    this_fail_state = fail_state;
    insert_state(expr_start, '\0', end_state+2, 0);
    insert_state(fail_state, '\0', -1, -1);
    end_state += 2;
    printf("expr_start: %2d\n",expr_start);
    term();
    if (lexch() == RE_OR) {
        printf("OR: last_expr: %2d,end_state: %2d\n",last_expr,end_state);
        or_pending = true;
        this_fail_state = fail_state;
        insert_state(end_state,'\0',end_state+1,0);
        complete_state = end_state++;
        expr();
        get_state(this_fail_state)->next1 = last_expr;
        get_state(complete_state)->next1 = end_state-1;
    }
    else {
        unlexch();
    }
    printf("lexnext: %s\n",lexnext);
    if (*lexnext == '\0' && rp_pending != 0) error(badparen);
    last_expr = expr_start;
    closure_fail_state = this_fail_state;
    printf("expr end\n");
    insert_state(++end_state,'\0',0,0);
    fail_state = -1;
}

void
term(void)
{
    char c;

    factor();
    c = lexch();
    if (c != '\0' && c != RE_RP && c != RE_OR) {
        unlexch();
        term();
    }
    else if (c == RE_RP) {
        if (rp_pending > 0) {
            rp_pending--;
        }
        else {
            error(badparen);
        }
    }
    else if (c == RE_OR) {
        unlexch();
    }
}

void
factor(void)
{
    char c = lexch();
    struct s_entry* st;

    printf("factor: c: %c\n",c);
    if (or_pending && c <= 0 && c != RE_LP)
        error("malformed or");
    or_pending = false;

    if (c == RE_LP) {
        rp_pending++;
        expr();
    }
    else if (c == RE_CL) {
        /* "(a|b)*" sets wrong fail state 3,  should be 7
        */

        printf("closure: last_expr: %2d, end_state: %2d, fail_state: %2d, "
               "closure_fail_state: %2d\n",
               last_expr, end_state, fail_state, closure_fail_state);
        if (get_state(last_expr)->event == '\0') {
            // closure of expression
            get_state(closure_fail_state)->next1 = end_state;
            get_state(end_state-1)->next1 = last_expr;
        }
        else {
            // closure of single character
            get_state(last_expr)->next2 = end_state;
            get_state(last_expr)->next1 = last_expr;
        }
    }
    else if (c > '\0')  {
        printf("factor char: last_expr: %2d, end_state: %2d, fail_state: %2d\n",
               last_expr, end_state, fail_state);
        insert_state(end_state, c, end_state+1, fail_state);
        closure_fail_state = fail_state;
        last_expr = end_state++;
    }
    else {
        unlexch(); //error("wtf!");
    }
}

bool
match1(char* s, int state)
{
    struct s_entry* st;
    int next_state;
    char next_event;

    st = get_state(state);
    printf("s: '%s',state: %2d\n",s,state);
    do {
        if (st->next1 == 0 && st->next2 == 0) return true;
        if (st->event == '\0') {
            // if null event, use next1 as next state unconditionally
            st = get_state(st->next1);
            if (st->next1 == -1) return false;
        }
    } while (st->event == '\0');
    if (*s == st->event) {
        next_state = st->next1;
    }
    else {
        next_state = st->next2;
        if (next_state != -1) s--; // preserve non-matched char
    }

    if (next_state == -1) {
        if (*(s+1) == '\0') return false;
        next_state = 0;
    }

    return match1(++s,next_state);
}

bool
match(char* s, int state)
{
    struct s_entry* st;
    int back_track = -1,
        i = 0,
        new_state;
    printf("s: %s\n",s);
    while (s[i] != '\0') {
        st = get_state(state);
        printf("i: %2d, s[i]: '%c',state: %2d\n",i,s[i],state);
        do {
            if (st->next1 == 0 && st->next2 == 0) return true;
            if (st->next1 == -1) return false;
            if (st->event == '\0') {
                // if null event, use next1 as next state
                // unconditionally
                new_state = st->next1;
                st = get_state(new_state);
                back_track = -1;
                printf("state transition: %2d => %2d\n",state, new_state);
            }
        } while (st->event == '\0');

        if (back_track < 0) back_track = i;
        printf("i: %2d, s[i]: '%c', st->event: '%c'\n", i, s[i], st->event);
        if (s[i] == st->event) {
            state = st->next1;
            i++;
        }
        else {
            state = st->next2;
            if (state == -1) return false;
            i = back_track;
            back_track = -1;
        }
    }
    return true;
}

int
main(int argc, char* argv[])
{
    char search[LEXBUFSIZE];
    char* s;

    if (argc > 1)
        lexbuf_init(argv[1]);
    else
        lexbuf_init("Z(A|B)CC*");

    expr();
    print_states(end_state);
    while (fgets(search,LEXBUFSIZE,stdin)) {
        if ((s = strrchr(search,'\n'))) *s = '\0';
        if (match(search,0))
            printf("found\n");
        else
            printf("not found\n");
    }
    return EXIT_SUCCESS;
}
