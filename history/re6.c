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

int end_state = 0;
int fail_state = 0;
int expr_start = 0;
int last_or_state = -1;

int rp_pending = 0;
bool or_pending = false;

void
expr(void)
{
    int this_expr_start, complete_state, last_expr_start;
    char c;
    struct s_entry* st;

    last_expr_start = expr_start;
    expr_start = end_state;
    insert_state(expr_start, '\0', end_state+1, -1);
    end_state++;
    printf("expr_start: %2d\n",expr_start);
    term();
    if (lexch() == RE_OR) {
        printf("OR1: expr_start: %2d,end_state: %2d\n",expr_start,end_state);
        or_pending = true;
        this_expr_start = expr_start;
        // fail state of this expression points to next expression
        if (get_state(end_state-1)->event > 0) {
            get_state(expr_start)->next2 = end_state;
        }
        else {
            //printf("end_state-1(%2d)->next1 = %2d\n",end_state-1,end_state);
            get_state(end_state-1)->next1 = end_state;
        }
        // remember complete state of this expression
        complete_state = (get_state(end_state-1)->event < 0)?end_state-2:
            end_state-1;
        expr();
        printf("OR2: this_expr_start: %2d, expr_start: %2d, end_state: %2d\n",
               this_expr_start, expr_start, end_state);
        // complete state of this expression goes to end_state
        printf("complete_state(%2d)->next1 = end_state (%2d)\n",
               complete_state, end_state);
        if (get_state(end_state-1)->event < 0)
            get_state(complete_state)->next1 = end_state-2;
        else
            get_state(complete_state)->next1 = end_state;
    }
    else {
        unlexch();
    }
    printf("lexnext: %s\n",lexnext);
    if (*lexnext == '\0' && rp_pending != 0) error(badparen);
    printf("expr end: expr_start: %2d. Previous expr: %2d, end_state: %2d\n",
           expr_start, last_expr_start, end_state);
    //write expression success/fail states if none exist
    if (get_state(end_state-1)->event > 0) {
        insert_state(end_state, -1, end_state+2, end_state+2);
        // fail state of this expression points to closed expression
        // fail state
        get_state(expr_start)->next2 = ++end_state;
        insert_state(end_state++,-1,-1,expr_start);
    }
    if (expr_start == 0) insert_state(end_state,'\0',0,0);
    expr_start = last_expr_start;
    //fail_state = expr_start+1;
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
    static int last_expr_start;

    printf("factor: c: %c\n",c);
    if (or_pending && c <= 0 && c != RE_LP)
        error("malformed or");
    or_pending = false;

    if (c == RE_LP) {
        rp_pending++;
        last_expr_start = end_state;
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
            printf("): last_expr_start: %2d, expr_start: %2d, end_state: %2d, "
                   " last expr (from end_state): %2d\n",
                   last_expr_start, expr_start, end_state,
                   get_state(end_state-1)->next2);
            // fail state of last expression before ) set to fail
            // state of enclosing expression (in case of following or)
            //get_state(get_state(end_state-1)->next2)->next2 =
            // expr_start;
            //expr_start = last_expr_start;
        }
        else {
            unlexch();
        }
    }
    else if (c == RE_CL) {
        printf("closure: last_expr_start: %2d, end_state: %2d, "
               "expr_start: %2d\n", last_expr_start, end_state, expr_start);
        printf("end_state: %2d, event: %2d\n",
               end_state, get_state(end_state)->event);

        //if (last_expr_start < 0) {
        // this check is not reliable,
        // ./re "((a|b)|(c|d)kk*)z"
        // next2 events for second k are wrong
        // also, backracking fails
        st = get_state(end_state-1);
        if (st->event > 0) {
            // closure of single character
            printf("single char: end_state-1: %2d\n", end_state-1);
            st->next1 = end_state-1;
            st->next2 = end_state;
        }
        else {
            // closure of expression;
            printf("last_or_state: %2d\n", last_or_state);
            // fail state of last expression set to succeed
            printf("writing end_state (%2d) to last_or_state+1 (%2d) next1\n",
               end_state, last_expr_start+1);
            get_state(end_state-1)->next1 = end_state;
                //}
            // success of last expression causes jump back to start
            get_state(end_state-2)->next1 = get_state(end_state-1)->next2;
            //insert_state(--end_state,'\0',0,0);
        }
    }
    else if (c > '\0')  {
        printf("factor char: last_expr_start: %2d, end_state: %2d, "
               "expr_start: %2d\n",
               last_expr_start, end_state, expr_start);
        // over-write previous end_state if a success state
        //st = get_state(end_state-1);
        //if (st->next1 == 0 && st->next2 == 0) end_state--;
        insert_state(end_state, c, end_state+1, expr_start);
        //insert_state(++end_state,'\0',0,0);
        end_state++;
    }
    else {
        unlexch(); //error("wtf!");
    }
}

bool
match(char* s, int state)
{
    struct s_entry* st;
    int back_track = -1,
        i = 0,
        new_state;
    bool follow_next2 = false,
        endstr = false;

    printf("s: %s\n",s);
    while (!endstr) {
        st = get_state(state);
        endstr = (s[i] == '\0');
        printf("i: %2d, s[i]: '%c',state: %2d\n",i,s[i],state);
        do {
            if (st->next1 == 0 && st->next2 == 0) return true;
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
                printf("state transition: %2d => %2d\n",state, new_state);
            }
        } while (st->event <= '\0');

        follow_next2 = false;
        if (back_track < 0) back_track = i;
        printf("i: %2d, s[i]: '%c', st->event: '%c'\n", i, s[i], st->event);
        if (s[i] == st->event) {
            state = st->next1;
            i++;
        }
        else {
            state = st->next2;
            follow_next2 = true;
            if (state == -1) {
                state = 0;
                i++;
                continue;
            }
            if (back_track >= 0) {
                i = back_track;
                back_track = -1;
            }
        }
      next:
        continue;
    };

    return false;
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
