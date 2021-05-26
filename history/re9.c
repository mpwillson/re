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

#include "re.h"
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

bool debug = false;

/* signed char is implementation dependent */
static char lexbuf[LEXBUFSIZE];
static char* lexnext;

/* forward decls */
static void term(void);
static void factor(void);
static void expr(void);

static void
error(char* msg)
{
    fprintf(stderr,"re: %s\n",msg);
    exit(EXIT_FAILURE);
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

static int end_state = 0;
static int rp_pending = 0;
static bool or_pending = false;
static int current_expr = 0;

static void
expr(void)
{
    int expr_start, complete_state, nested_expr_start = -1;
    char c;
    struct s_entry* st;

    expr_start = end_state;
    current_expr = expr_start;
    insert_state(expr_start, '\0', end_state+2, end_state+2);
    insert_state(expr_start+1, '\0', -1, -1);
    end_state += 2;
    if (debug) printf("expr_start: %2d\n",expr_start);
    term();
  start:
    c = lexch();
    switch (c) {
        case RE_OR:
            if (debug)
                printf("OR1: expr_start: %2d,end_state: %2d\n",
                       expr_start,end_state);
            or_pending = true;
            // fail state of this expression must point to start of
            // next
            get_state(expr_start+1)->next1 = end_state+2;
            complete_state = end_state-1;
            expr();
            if (debug)
                printf("OR2: expr_start: %2d, end_state: %2d\n",
                       expr_start, end_state);
            // complete state must point to end of or expressions
            get_state(complete_state)->next1 = end_state-1;
            break;
        case RE_LP:
            if (debug) printf("( preamble\n");
            nested_expr_start = end_state;
            expr();
            if (debug) printf("( postamble: expr: %2d, nested_expr: %2d\n",
                              expr_start, nested_expr_start);
            // throw fail state of nested expr to this expr
            // if fail state not already munged by OR
            st = get_state(nested_expr_start+1);
            if (st->next1 == -1) st->next1 = expr_start+1;
            c = lexch();
            if (c == RE_RP) {
                c = lexch();
                if (c == RE_CL) {
                    if (debug) printf("closure of bracketed expr\n");
                    // successful end returns to start
                    get_state(end_state-1)->next1 = nested_expr_start+2;
                    // any failure goes to end state to carry on
                    // uses fail state written by expr()
                    get_state(get_state(end_state-1)->next2)->next1 =
                        end_state;
                }
                else {
                    unlexch();
                }
            }
            else {
                unlexch();
            }
            goto start;

            break;
        case '\0':
            break;
        case RE_RP:
            if (debug) printf(") found: expr_start: %2d\n", expr_start);
            // need to handle post expr stuff

            unlexch(); // enclosing expr will deal
            break;
        default:
            unlexch();
            term();
            /* fprintf(stderr, "expr: found: %d/%c - ", c, c); */
            /* error("missing case handler\n"); */
    }

    if (debug) printf("lexnext: %s\n",lexnext);
    if (*lexnext == '\0' && rp_pending != 0) error(badparen);
    if (debug)
        printf("expr end: expr_start: %2d, end_state: %2d\n",
               expr_start, end_state);
    // rewrite all fail states to expr fail state ???
    for (int i = expr_start+2; i < end_state; i++) {
        st = get_state(i);
        if (st->next2 == -1) st->next2 = expr_start+1;
    }
    if (expr_start == 0) {
        insert_state(++end_state,'\0',0,0);
    }
    else if (get_state(end_state-1)->event > 0 ) {
        // write end state for expression, fail state is in next2
        // only the first expression closed writes this, since
        // we need the fail state of the last expr in an OR sequence
        // is a closure
        insert_state(end_state,'\0',end_state+1,expr_start+1);
        end_state++;
    }
}

static void
term(void)
{
    char c;

    factor();
    c = lexch(); unlexch();
    if (debug) printf("term: c = %d/'%c'\n",c,c);
    // test could be c > 0 (oh, except for RE_CL)
    if (c != '\0' && c != RE_OR && c != RE_RP  && c != RE_LP) {
        term();
    }
}

static void
factor(void)
{
    char c = lexch();
    struct s_entry* st;

    if (debug) printf("factor: current char: %c\n",c);
    if (or_pending && c <= 0 && c != RE_LP)
        error("malformed or");
    or_pending = false;

    if (c > '\0')  {
        if (debug)
            printf("factor: end_state: %2d, current_expr: %2d\n",
                   end_state, current_expr);
        // -1 default fail state replaced by expr fail state when
        // expression ends (in expr())
        insert_state(end_state, c, end_state+1, -1);
        end_state++;
    }
    else if (c == RE_CL) {
        if (debug) printf("single char closure: end_state: %2d\n", end_state);
        get_state(end_state-1)->next1 = end_state-1;
        get_state(end_state-1)->next2 = end_state;
    }
    else {
        if (debug) printf("factor else: c: %2d/'%c'\n",c,c);
        unlexch();
    }
}

bool
compile(char* re_str)
{
    lexbuf_init(re_str);
    expr();
    return true;
}

struct re_matched*
match(char* s, int state)
{
    struct s_entry* st;
    static struct re_matched matched;
    int back_track = -1,
        i = 0,
        new_state,
        match_start;
    bool follow_next2 = false,
        endstr = false;

    if (debug) printf("s: %s\n",s);
    while (!endstr) {
        if (state == 0) match_start = -1;
        st = get_state(state);
        endstr = (s[i] == '\0');
        if (debug) printf("i: %2d, s[i]: '%c',state: %2d\n",i,s[i],state);
        do {
            if (st->next1 == 0 && st->next2 == 0) {
                matched.start = match_start;
                matched.end = i;
                return &matched;
            }
            if (st->next1 == -1 || (follow_next2 && st->next2 == -1)) {
                state = 0;
                i++;
                goto next;
            }
            if (st->event <= '\0') {
                // if null event, use next1 as next state
                // unconditionally
                new_state = (follow_next2)?st->next2:st->next1;
                follow_next2 = false;
                st = get_state(new_state);
                back_track = -1;
                if (debug)
                    printf("state transition: %2d => %2d\n",state, new_state);
            }
        } while (st->event <= '\0');

        follow_next2 = false;
        if (back_track < 0) back_track = i;
        if (debug)
            printf("i: %2d, s[i]: '%c', st->event: '%c'\n", i, s[i], st->event);
        if (s[i] == st->event) {
            state = st->next1;
            if (match_start < 0) match_start = i;
            i++;
        }
        else {
            // only backtrack for alternates (not closures)
            if (back_track >= 0 ) {
                if (state != st->next1 ||
                    get_state((get_state(st->next2)->next2)-1)->next1 > state) {
                    i = back_track;
                    back_track = -1;
                }
            }
            state = st->next2;
            // if next state is a transition, defer endstr
            endstr = (endstr && get_state(state)->event > 0);
            //follow_next2 = true;
            if (state == -1) {
                state = 0;
                i++;
                continue;
            }
        }
      next:
        continue;
    };
    return NULL;
}
