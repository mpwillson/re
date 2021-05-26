/*
 * Regular Expressions
 *
 * Grammer:
 * <expression> ::= <term> | <term> '|' <expression>
 * <term>       ::= <factor> | <factor> <term>
 * <factor>     ::= '('<expression>')' | v | <factor>'*'
 */

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
int expr_start = 0;
int last_or_state = 0;

int rp_pending = 0;
bool or_pending = false;

void
expr(void)
{
    int this_expr_start, complete_state;
    char c;
    struct s_entry* st;

    this_expr_start = expr_start;
    expr_start = end_state;
    fail_state = expr_start+1;
    insert_state(expr_start, '\0', end_state+2, 0);
    insert_state(fail_state, '\0', -1, -1);
    end_state += 2;
    printf("expr_start: %2d\n",expr_start);
    term();
    if (lexch() == RE_OR) {
        printf("OR1: expr_start: %2d,end_state: %2d\n",expr_start,end_state);
        or_pending = true;
        last_or_state = end_state;
        // fail state of current expression points to next expression
        get_state(expr_start+1)->next1 = end_state;
        // remember real start of this expression
        complete_state = end_state-1;
        expr();
        // success state of last match goes to end_state
        printf("writing end_state (%2d) to end_state-1 (%2d) next1\n",
               end_state, end_state-1);
        get_state(end_state-1)->next1 = end_state;
        // success state of begin_state goes to end_state
        printf("writing end_state (%2d) to complete_state (%2d) next1\n",
               end_state, complete_state);

        get_state(complete_state)->next1 = end_state;
        printf("OR2: this_expr_start: %2d, expr_start: %2d, end_state: %2d\n",
               this_expr_start, expr_start, end_state);
    }
    else {
        unlexch();
    }
    printf("lexnext: %s\n",lexnext);
    if (*lexnext == '\0' && rp_pending != 0) error(badparen);
    printf("expr end: expr_start: %2d. Next: %2d\n",expr_start, this_expr_start);
    last_expr = expr_start;
    printf("writing expr_start (%2d) to end_state (%2d) next2\n",
           expr_start, end_state);
    insert_state(end_state,'\0',end_state+1,expr_start);
    if (expr_start == 0) insert_state(++end_state,'\0',0,0);
    expr_start = this_expr_start;
    fail_state = expr_start+1;
    /* st = get_state(end_state); */
    /* if (st->next1 != 0 && st->next2 != 0) { */
}

void
term(void)
{
    char c;

    factor();
    c = lexch(); unlexch();
    if (c != '\0' && c != RE_OR && c != RE_RP) {
        term();
    }
}

void
factor(void)
{
    char c = lexch();
    struct s_entry* st;
    static int last_expr_start = -1;

    printf("factor: c: %c\n",c);
    if (or_pending && c <= 0 && c != RE_LP)
        error("malformed or");
    or_pending = false;

    if (c == RE_LP) {
        rp_pending++;
        expr();
        printf("after expression in LP: end_state: %2d, expr_start: %2d\n",
               end_state, expr_start);
        c = lexch();
        if (c == RE_RP) {
            if (rp_pending > 0) {
                rp_pending--;
            }
            else {
                error(badparen);
            }
            printf("): expr_start: %2d, end_state: %2d\n",
                   expr_start, end_state);
            last_expr_start = get_state(end_state)->next2;
            //insert_state(end_state,RE_RP,-1,-1);
            last_expr = end_state;
        }
        else {
            unlexch();
        }
    }
    else if (c == RE_CL) {
        printf("closure: last_expr_start: %2d, end_state: %2d, "
               "expr_start: %2d\n", last_expr_start, end_state, expr_start);
        st = get_state(end_state-1);
        if (last_expr_start < 0) {
            // closure of single character
            st->next1 = end_state-1;
            st->next2 = end_state;
        }
        else {
            printf("last_or_state: %2d\n", last_or_state);
            // closure of expression; end_state-1 contains expr_start
            // of last expression
            // fail state of last expression set to succeed
            printf("writing end_state (%2d) to last_or_state+1 (%2d) next1\n",
               end_state, last_expr_start+1);
            get_state(last_or_state+1)->next1 = end_state;
            // success of last expression causes jump back to start
            get_state(end_state-1)->next1 = last_expr_start;
            //insert_state(--end_state,'\0',0,0);
            last_expr_start = -1;
        }
    }
    else if (c > '\0')  {
        printf("factor char: last_expr_start: %2d, end_state: %2d, "
               "expr_start: %2d\n",
               last_expr, end_state, expr_start);
        insert_state(end_state, c, end_state+1, expr_start+1);
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
