/*
 * Regular Expressions
 *
 * Grammer:
 * <expression> ::= <term> | <term> '|' <expression>
 * <term>       ::= <factor> | <factor> <term>
 * <factor>     ::= '('<expression>')' | v | <factor>'*'
 *
 * Version 12 - testing use of current_expr: doesn't seem to work
 *
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

char* badparen = "unbalanced parenthesis";

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
static int current_fail_state = 0;

static void
expr(void)
{
    int expr_start = 0, complete_state, nested_expr_start = -1,
        previous_fail_state;
    char c;
    struct s_entry* st;

    expr_start = end_state;
    if (expr_start == 0) {
        insert_state(expr_start++, '\0', 1, -1);
        end_state++;
    }
    insert_state(expr_start, '\0', ++end_state, current_fail_state);
    if (debug) printf("expr: expr_start: %2d, current_fail_state: %2d, "
                      "end_state: %2d\n",
                      expr_start, current_fail_state, end_state);
    previous_fail_state = current_fail_state;
    current_fail_state = expr_start;
    term();
    c = lexch();
    if (c == RE_OR) {
            if (debug)
                printf("OR1: expr_start: %2d,end_state: %2d\n",
                       expr_start,end_state);
            or_pending = true;
            // fail state of this expression must point to start of
            // next
            if (expr_start != 0) get_state(expr_start)->next2 = end_state;
            complete_state = end_state-1;
            expr();
            if (debug)
                printf("OR2: expr_start: %2d, complete_state: %2d, "
                       "end_state: %2d\n",
                       expr_start, complete_state, end_state);
            // complete state of this or must point to end of or expressions
            get_state(complete_state)->next1 = end_state-1;
    }
    else {
        if (debug) printf("expr: pushback: %2d\n",c);
        unlexch();
    }

    if (debug) printf("lexnext: %s\n",lexnext);
    if (*lexnext == '\0' && rp_pending != 0) error(badparen);
    if (debug)
        printf("expr end: expr_start: %2d, current_fail_state: %2d, "
               "end_state: %2d\n",
               expr_start, current_fail_state, end_state);
    if (expr_start == 1) {
        // outmost expression
        if (rp_pending != 0) error(badparen);
        // fake brackets
        int last_expr_start = get_state(end_state-1)->next2;
        st = get_state(last_expr_start);
        if (debug) printf("last_expr_start: %2d\n", last_expr_start);
        //if (st->next2 == last_expr_start-1 ||
        if (get_state(end_state-1)->event != '\0') st->next2 = expr_start-1;
        insert_state(++end_state,'\0',0,0);
    }
    else if (get_state(end_state-1)->event > 0) {
        // write end state for expression, with expr start in next2
        // only the first expression closed writes this, since
        // we need the fail state of the last expr in an OR sequence
        // is a closure
        insert_state(end_state,'\0',end_state+1,expr_start);
        end_state++;
    }
    current_fail_state = previous_fail_state;
}

static void
term(void)
{
    char c;

    factor();
    c = lexch(); unlexch();
    if (debug) printf("term: c = %d/'%c'\n",c,c);
    if (c != '\0' && c != RE_OR && c != RE_RP) {
        term();
    }
}

static void
factor(void)
{
    char c = lexch();
    struct s_entry* st;
    int nested_expr_start;

    if (debug) printf("factor: current char: %2d/'%c'\n",c,c);
    if (or_pending && c <= 0 && c != RE_LP)
        error("malformed or");
    or_pending = false;

    if (c == RE_LP) {
        rp_pending++;
        nested_expr_start = end_state;
        if (debug) printf("factor: ( preamble: current_fail_state: %2d, "
                          "nested_expr_start: %2d\n",
                          current_fail_state, nested_expr_start);
        insert_state(nested_expr_start,'\0',++end_state,current_fail_state);
        expr();
        if (debug) printf("( postamble: current_fail_state: %2d, "
                          "nested_expr: %2d\n",
                          current_fail_state, nested_expr_start);
        // throw fail state of final (or only) expr to this expr
        get_state(get_state(end_state-1)->next2)->next2  = nested_expr_start;
        c = lexch();
        if (c != RE_RP) error(badparen);
        rp_pending--;
    }
    else if (c > '\0') {
        if (debug)
            printf("factor V: end_state: %2d, current_fail_state: %2d\n",
                   end_state, current_fail_state);
        insert_state(end_state, c, end_state+1, current_fail_state);
        end_state++;
    }
    else {
        error("malformed expression");
        if (debug) printf("factor else: c: %2d/'%c'\n",c,c);
        unlexch();
    }

    // get last state (either char or expr end)
    st = get_state(end_state-1);
    c = lexch();
    if (c == RE_CL) {
        if (st->event > '\0') {
            if (debug)
                printf("single char closure: end_state: %2d\n", end_state);
            st->next1 = end_state-1;
            st->next2 = end_state;
        }
        else {
            if (debug) printf("closure of bracketed expr\n");
            // successful end returns to start
            st->next1 = nested_expr_start;
            // any failure goes to end state to carry on
            // uses fail state written by expr()
            get_state(nested_expr_start)->next2 = end_state;
        }
    }
    else {
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
match(char* s)
{
    struct s_entry* st;
    static struct re_matched matched;
    int state = 1,
        back_track = -1,
        i = 0,
        new_state,
        match_start = -1;
    bool follow_next2 = false,
        endstr = false;

    if (debug) printf("s: %s\n",s);
    while (!endstr) {
        st = get_state(state);
        endstr = (s[i] == '\0');
        if (debug) printf("i: %2d, s[i]: '%c',state: %2d\n",i,s[i],state);
        do {
            if (st->next1 == 0 && st->next2 == 0) {
                matched.start = match_start;
                matched.end = i;
                return &matched;
            }
            if (st->next2 == -1) {
                follow_next2 = false;
                match_start = -1;
                state = 1;
                i++;
                goto next;
            }
            if (st->event <= '\0') {
                // if null event, use next1 as next state
                // unconditionally
                new_state = (follow_next2)?st->next2:st->next1;
                // follow next2 will going back up expr chain
                // forward (higher) state means success is still possible
                follow_next2 = follow_next2 && (new_state < state);
                st = get_state(new_state);
                back_track = -1;
                if (debug)
                    printf("state transition: %2d => %2d\n",state, new_state);
                state = new_state;
            }
        } while (st->event <= '\0');

        follow_next2 = false;
        if (back_track < 0) back_track = i;
        if (debug)
            printf("i: %2d, s[i]: '%c', st->event: '%c'\n", i, s[i], st->event);
        if (s[i] == st->event) {
            state = st->next1;
            if (debug) printf("success: new_state: %2d\n", state);
            if (match_start < 0) match_start = i;
            i++;
        }
        else {
            // only backtrack for alternates (not closures)
            new_state = st->next2;
            if (debug) printf("fail: new_state: %2d\n", new_state);
            if (back_track >= 0 ) {
                if (new_state < state && get_state(new_state)->next2 > state) {
                    if (debug) printf("backtrack to %2d\n", back_track);
                    i = back_track;
                    back_track = -1;
                }
            }
            // if next state is a transition, defer endstr
            endstr = (endstr && get_state(new_state)->event > 0);
            // only use next2 for next state when new state is less (a
            // real fail, not a closure fail)
            follow_next2 = (new_state < state);
            if (follow_next2) match_start = -1;
            if (new_state == -1) {
                state = 0;
                i++;
                continue;
            }
            state = new_state;
        }
      next:
        continue;
    };
    return NULL;
}
