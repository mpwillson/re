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
static int expr_start = 0;
//static int last_expr_start = 0;

static int rp_pending = 0;
static bool or_pending = false;

static void
expr(void)
{
    int this_expr_start, complete_state, last_expr_start;
    char c;
    struct s_entry* st;

    last_expr_start = expr_start;
    expr_start = end_state;
    insert_state(expr_start, '\0', end_state+2, end_state+2);
    insert_state(expr_start+1, '\0', -1, -1);
    end_state += 2;
    if (debug)
        printf("expr_start: %2d\n",expr_start);
    term();
    if (lexch() == RE_OR) {
        if (debug)
            printf("OR1: expr_start: %2d,end_state: %2d\n",
                   expr_start,end_state);
        or_pending = true;
        this_expr_start = expr_start;
        // fail state of this expression points to next expression
        if (get_state(end_state-1)->event > 0) {
            get_state(expr_start+1)->next1 = end_state;
        }
        else {
            if (debug)
                printf("end_state-1(%2d)->next1 = %2d\n",end_state-1,end_state);
            get_state(end_state-1)->next1 = end_state;
        }
        // remember complete state of this expression
        complete_state = (get_state(end_state-1)->event < 0)?end_state-2:
            end_state-1;
        expr();
        if (debug)
            printf("OR2: this_expr_start: %2d, expr_start: %2d, "
                   "end_state: %2d\n",
                   this_expr_start, expr_start, end_state);
        // complete state of this expression goes to end_state
        if (debug)
            printf("complete_state(%2d)->next1 = end_state (%2d)\n",
               complete_state, end_state);
        if (get_state(end_state-1)->event < 0)
            // previous expression
            get_state(complete_state)->next1 = end_state-2;
        else
            // single character term
            get_state(complete_state)->next1 = end_state;
    }
    else {
        unlexch();
    }
    if (debug) printf("lexnext: %s\n",lexnext);
    if (*lexnext == '\0' && rp_pending != 0) error(badparen);
    if (debug)
        printf("expr end: expr_start: %2d. Previous expr: %2d, "
               "end_state: %2d\n",
               expr_start, last_expr_start, end_state);
    //write expression success/fail states if none exist
    if (get_state(end_state-1)->event > 0) {
        insert_state(end_state, -1, end_state+2, expr_start);
        // fail state of this expression points to closed expression
        // fail state
        get_state(expr_start+1)->next1 = end_state+1;
        end_state++;
        insert_state(end_state++,-1,last_expr_start+1,last_expr_start+1);
    }
    if (expr_start == 0) {
        insert_state(1,-1,-1,-1);
        insert_state(++end_state,'\0',0,0);
    }
    expr_start = last_expr_start;
}

static void
term(void)
{
    char c;

    factor();
    c = lexch(); unlexch();
    if (c != '\0' && c != RE_OR && c != RE_RP) {
        term();
    }
}

static void
factor(void)
{
    char c = lexch();
    struct s_entry* st;
    static int last_expr_start;
    int xxx;

    if (debug) printf("factor: current char: %c\n",c);
    if (or_pending && c <= 0 && c != RE_LP)
        error("malformed or");
    or_pending = false;

    if (c == RE_LP) {
        rp_pending++;
        last_expr_start = expr_start;//end_state;
        expr_start = end_state;
        // write bracketed expression preamble
        if (debug) printf("(: state: %2d\n,", end_state);
        insert_state(end_state, '\0', end_state+2, end_state+2);
        end_state++;
        insert_state(end_state, '\0', -1, -1);
        end_state++;
        expr();
        if (debug)
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
            if (debug)
                printf("): last_expr_start: %2d, expr_start: %2d, "
                       "end_state: %2d, last expr (from end_state): %2d\n",
                       last_expr_start, expr_start, end_state,
                       get_state(end_state-1)->next2);
            // overwrite the complete/fail states of the
            // previous expression
            end_state -= 2;
            // remember expr start of last expression
            xxx = get_state(end_state)->next2;
            // write bracketed expression complete and fail states
            insert_state(end_state,-1,end_state+2,xxx);
            insert_state(++end_state, -1, last_expr_start+1, last_expr_start+1);
            // fail of previous expression goes to this fail state
            get_state(last_expr_start+1)->next1 = end_state++;
            expr_start = last_expr_start;
        }
        else {
            unlexch();
        }
    }
    else if (c == RE_CL) {
        if (debug)
            printf("closure: last_expr_start: %2d, end_state: %2d, "
                   "expr_start: %2d\n", last_expr_start, end_state, expr_start);
        if (debug)
            printf("end_state: %2d, event: %2d\n",
                   end_state, get_state(end_state)->event);
        st = get_state(end_state-1);
        if (st->event > 0) {
            // closure of single character
            if (debug)
                printf("single char: end_state-1: %2d\n", end_state-1);
            st->next1 = end_state-1;
            st->next2 = end_state;
        }
        else {
            // closure of expression;
            // fail state of last expression set to succeed
            if (debug)
                printf("end_state-1(%2d)->next1 = end_state (%2d)\n",
                       end_state-1, end_state);
            get_state(end_state-1)->next1 = end_state;
                //}
            // success of last expression causes jump back to start
            if (debug)
                printf("end_state-2(%2d)->next1 = get_state(end_state-1)->next2"
                       " (%2d)\n",
                       end_state-2, get_state(end_state-1)->next2);
            // last complete state next2 has expr_start
            get_state(end_state-2)->next1 = get_state(end_state-2)->next2;
            //insert_state(--end_state,'\0',0,0);
        }
    }
    else if (c > '\0')  {
        if (debug)
            printf("factor char: last_expr_start: %2d, end_state: %2d, "
                   "expr_start: %2d\n",
                   last_expr_start, end_state, expr_start);
        insert_state(end_state, c, end_state+1, expr_start+1);
        end_state++;
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
