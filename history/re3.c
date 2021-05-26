
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

int rp_pending = 0;
bool or_pending = false;

void
expr(void)
{
    int expr_start;
    char c;
    struct s_entry* st;

    insert_state(end_state,'\0',end_state+1,0);
    expr_start = end_state++;
    //last_expr = end_state++;
    printf("expr_start: %2d\n",expr_start);
    term();
    if (lexch() == RE_OR) {
        printf("last_expr: %2d,end_state: %2d\n",last_expr,end_state);
        int this_expr = last_expr;
        or_pending = true;
        get_state(last_expr)->next2 = last_expr+1;
        expr();
        get_state(this_expr)->next1 = end_state-1;
        //get_state(last_expr)->next1 = end_state-1;
        //insert_state(last_end_state,'\0',end_state,end_state);
    }
    else {
        unlexch();
    }
    printf("lexnext: %s\n",lexnext);
    if (*lexnext == '\0' && rp_pending != 0) error(badparen);
    last_expr = expr_start;
    printf("expr end\n");
    insert_state(end_state,'\0',end_state+1,0);
    insert_state(++end_state,'\0',0,0);
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
/*
 *  /re "z(a|bb|ccc)z"
 *  Fails, as first b has -1 as next2 state
 */

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
        printf("closure: last_expr: %2d, end_state: %2d\n",
               last_expr, end_state);
        // complete state returns to start
        get_state(end_state-1)->next1 = last_expr;
        // if concat, set first state to end state
        // else, if alternate set all fail states to end state
        st = get_state(get_state(last_expr)->next1);
        if (st->next2 == -1) {
            st->next2 = end_state;
        }
        else {
            for (int i=last_expr; i<end_state; i++) {
                st = get_state(i);
                if (st->next2 == -1) st->next2 = end_state;
            }
        }
    }
    else if (c > '\0')  {
        printf("factor char: last_expr: %2d, end_state: %2d\n",last_expr,
               end_state);
        insert_state(end_state,c,end_state+1,-1);
        last_expr = end_state++;
        //end_state++;
        //insert_state(end_state,'\0',end_state+1,0); // complete state
        //insert_state(end_state,'\0',0,0); // end state
    }
    else {
        unlexch(); //error("wtf!");
    }
}

bool
match(char* s, int state)
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

    return match(++s,next_state);
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
