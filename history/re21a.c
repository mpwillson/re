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
 * Version 21a
 * Defect in matching patterns like "z(a.*|b)z".  With the search
 * string zzaaaaaaaaaaaz, the two z's at the start are included in the
 * matched string; should be only one z, of course.
 * Hmm, looks like a fundamental problem in the machine generation,
 * since zbbz also matches, since the failure of b to match z causes
 * the machine to restart at 1.  I can't see how to distinguish the
 * attempt to match the last z is from the . closure or the b match.
 *
 *
 * Revised to handle sm.c/sm.h changes since Version 21.
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
static void term(void);
static void factor(void);
static void expr(void);

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

static int end_state = 0;
static int current_fail_state = 0;
static int expr_final_state = 0;
static int dot_state = -1;
static int rp_pending = 0;
static bool or_pending = false;

static void
expr(void)
{
    int expr_start = 0, complete_state, nested_expr_start = -1,
        previous_fail_state;
    char c;
    struct sm_entry* st;

    expr_final_state = -1;
    expr_start = end_state;
    if (expr_start == 0) {
        if (!sm_insert(expr_start++, '\0', 1, -1)) error(RE_ERR_MEM);
        end_state++;
    }
    if (!sm_insert(expr_start, '\0', ++end_state, current_fail_state))
        error(RE_ERR_MEM);
    DEBUG("expr", expr_start, current_fail_state, end_state);
    previous_fail_state = current_fail_state;
    current_fail_state = expr_start;
    term();
    c = lexch();
    if (c == RE_OR) {
        DEBUG("OR1", expr_start, end_state, 0);
        or_pending = true;
        // fail state of this expression must point to start of
        // next
        if (expr_start != 0) sm_state(expr_start)->next2 = end_state;
        complete_state = end_state-1;
        expr();
        DEBUG("OR2", expr_start, complete_state, end_state);
        // complete state of this or must point to end of or expressions
        sm_state(complete_state)->next1 = end_state-1;
    }
    else {
        unlexch();
    }

    if (debug) printf("lexnext: %s\n",lexnext);
    if (*lexnext == '\0' && rp_pending != 0) error(RE_ERR_UP);
    DEBUG("expr end", expr_start, current_fail_state, end_state);
    st = sm_state(end_state-1);
    if (expr_start == 1) {
        // outmost expression
        if (rp_pending != 0) error(RE_ERR_UP);
        // fake brackets; fail state of last_expr_state set to expr
        // containing expr.
        int last_expr_start = st->next2;
        st = sm_state(last_expr_start);
        DEBUG("expr final", last_expr_start, dot_state, 0);
        if (st) st->next2 = (dot_state < 0)?expr_start-1:dot_state;
        if (!sm_insert(end_state,'\0',0,0)) error(RE_ERR_MEM);
    }
    else if (st->event > 0 || st->event == RE_CL || st->event == RE_EOL) {
        // write end state for expression, with expr start in next2
        // only the first expression closed writes this, since
        // we need the fail state of the last expr in an OR sequence
        // if a closure.  The expr end state is used to detect type of
        // closure (if required).  It allows the whole set of
        // expressions with brackets to be closed over.
        expr_final_state = expr_start;
        if (!sm_insert(end_state,'\0',end_state+1,expr_start))
            error(RE_ERR_MEM);
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
    struct sm_entry* st;
    int nested_expr_start;

    DEBUGV("factor: current char: %2d/'%c'\n",c,c);
    if (or_pending && c <= 0 && c != RE_LP && c != RE_EOL && c != RE_BOL)
        error(RE_ERR_EX);
    or_pending = false;

    if (c == RE_LP) {
        rp_pending++;
        nested_expr_start = end_state;
        DEBUG("( preamble", current_fail_state, nested_expr_start, 0);
        if (!sm_insert(nested_expr_start,'\0',++end_state,
                          current_fail_state)) error(RE_ERR_MEM);
        expr();
        DEBUG("( postamble", expr_final_state, nested_expr_start, 0);
        // throw fail state of final (or only) expr to this expr
        sm_state(expr_final_state)->next2 = nested_expr_start;
        c = lexch();
        if (c != RE_RP) error(RE_ERR_UP);
        rp_pending--;
    }
    else if (c > '\0' || c == RE_BOL || c == RE_EOL || c == RE_DOT) {
        DEBUG("factor V", end_state, current_fail_state, 0);
        if (dot_state > 0 && sm_state(end_state-1)->event == RE_CL) {
            // closure of dot preceeded this char
            if (!sm_insert(end_state, c, end_state+1, dot_state))
                error(RE_ERR_MEM);
             dot_state = -1;
        }
        else {
            if (!sm_insert(end_state, c, end_state+1, current_fail_state))
                error(RE_ERR_MEM);
        }
        end_state++;
    }
    else {
        error(RE_ERR_EX);
    }

    // get last state (either char or expr end)
    st = sm_state(end_state-1);
    c = lexch();
    if (c == RE_CL) {
        if (st->event > '\0') {
            DEBUG("single char closure", end_state, 0, 0);
            st->next1 = end_state-1;
            st->next2 = end_state;
        }
        else if (st->event == RE_DOT) {
            DEBUG("dot closure", end_state, 0, 0);
            dot_state = end_state-1;
        }
        else if (st->event <= '\0') {
            DEBUG("expr closure", nested_expr_start, expr_final_state, 0);
            // successful end returns to start of first contained expr
            sm_state(end_state-1)->next1 = nested_expr_start+2;
            // failure on first closure character goes to end state to
            // carry on (for simple sequence of characters)
            sm_state(expr_final_state+1)->next2 = end_state;
        }
        // protect closure from following OR
        // event of RE_CL to ensure expr still writes expr trailer
        // NB An RE like (a*|b) is pointless as the first
        // alternate will always be a success (took me a while to
        // figure that out). Write it as (aa*|b)
        if (!sm_insert(end_state,RE_CL,end_state+1,end_state+1))
            error(RE_ERR_MEM);
        end_state++;
    }
    else {
        unlexch();
    }
}

void
optimise(void)
{
    int s = 0, f;
    struct sm_entry* st;

    st = sm_state(s);
    while (st->next1 != 0 && st->next2 != 0) {
        f = s;
        while (st->next1 == f+1) {
            f = st->next1;
            st = sm_state(st->next1);
            if (st->event > '\0' || st->event == RE_EOL
                || st->event == RE_BOL || st->event == RE_DOT) break;
       }

       if (f > s+1)
           sm_state(s)->next1 = f;
       else if (s == f)
           f++;
        s = f;
        st = sm_state(s);
    }
}

struct sm_fsm*
re_compile(char* re_str, int flags)
{
    int error_code;

    if ((error_code = setjmp(env)) == 0) {
        if (!sm_init()) {
            re_error_code = RE_ERR_INIT;
            return NULL;
        }
        lexbuf_init(re_str);
        expr();
        if (flags & RE_OPT) optimise();
    }
    return (error_code == 0)?sm_get():NULL;
}

/* Consider re-writing match */
struct re_matched*
re_match(struct sm_fsm* fsm, char* s)
{
    struct sm_entry* st, *machine;
    static struct re_matched matched;
    int state = 0,
        ntransitions = 0,
        back_track = -1,
        i = 0,
        new_state,
        match_start = -1,
        bol_anchor = 0,
        eol_anchor = 0;

    bool follow_next2 = false,
        endstr = false,
        force_restart = false,
        dot_closure = false;

    sm_set(fsm);
    machine = fsm->fsm;
    if (debug) printf("s: %s\n",s);
    re_error_code = 0;
    while (!endstr) {
        if (ntransitions > MAX_TRANSITIONS) {
            // probably a state machine loop
            re_error_code = RE_ERR_STL;
            return NULL;
        }
        st = sm_state(state);
        endstr = (s[i] == '\0');
        DEBUGV("i: %2d, s[i]: '%c',state: %2d\n",i,s[i],state);
        while (st->event <= '\0'  && st->event != RE_DOT) {
            if (st->next1 == 0 && st->next2 == 0) {
                if (!eol_anchor || (eol_anchor && s[i] == '\0')) {
                    matched.start = match_start;
                    matched.end = i;
                    return &matched;
                }
                else {
                    // match did not occur at end of line
                    force_restart = true;
                }
            }
            if ((follow_next2 && st->next2 == -1) || force_restart) {
                force_restart = false;
                follow_next2 = false;
                match_start = -1;
                state = 0;
                i++;
                goto next;
            }
            if (st->event <= '\0') {
                // note anchor expr start states
                if (st->event == RE_BOL) bol_anchor = st->next2;
                if (st->event == RE_EOL) eol_anchor = st->next2;
                // if null event, use next1 as next state
                // unconditionally
                new_state = (follow_next2)?st->next2:st->next1;
                // follow_next2 set going back up expr chain
                // forward (higher) state means success is still possible
                follow_next2 = follow_next2 && (new_state < state);
                // don't kill backtrack for closure
                if (st->event != RE_CL) back_track = -1;
                // dot closure?
                st = sm_state(new_state);
                if (st->event == RE_DOT &&
                    sm_state(new_state+1)->event == RE_CL) {
                    // consume dot char that failed
                    i++;
                    DEBUG("dot/closure target", i, back_track, 0);
                }
                DEBUG("state transition", state, new_state, 0);
                state = new_state;
                ntransitions++;
            }
        }

        follow_next2 = false;
        if (back_track < 0) back_track = i;
        DEBUGV("at: i: %2d, s[i]: '%c', st->event: '%c'\n", i, s[i], st->event);
        if (s[i] == st->event || st->event == RE_DOT) {
            if (bol_anchor && !(match_start == -1  && i == 0))
               return NULL;
             else
                bol_anchor = 0;
            new_state = st->next1;
            DEBUG("success", new_state, 0, 0);
            if (match_start < 0) match_start = i;
            // forward state elision (via optimiser) means backtrack
            // is dead
            if (new_state > state+1) back_track = -1;
            // if dot closure, don't consume char, leave to fail at
            // next state
            dot_closure = (st->event == RE_DOT &&
                            sm_state(state+1)->event == RE_CL);
            if (!dot_closure) i++;
            state = new_state;
        }
        else {
            //bool alternate_fail = (st->next1 > state+1);
            new_state = st->next2;
            // is fail target a dot?
            bool dot_fail = (sm_state(new_state)->event == RE_DOT);
            // consume char encountered by dot (non-closure)
            if (dot_closure) i++;
            // only use next2 for next state when new state is less
            follow_next2 = (new_state < state) &&
                (sm_state(new_state)->event != RE_DOT);
            st = sm_state(new_state);
            DEBUG("fail", state, new_state, dot_fail);
            if ((!follow_next2 && st->event == RE_CL) || dot_fail) {
                DEBUG("expr closure", i, 0, 0);
            }
            else {
                // only backtrack for alternates (not closures)
                // turn off bol_anchor that belong to this expr
                if (bol_anchor == sm_state(state)->next2) bol_anchor = 0;
                if (back_track >= 0) {
                    DEBUG("backtrack", back_track, 0, 0);
                    i = back_track;
                    back_track = -1;
                }
            }
            // if next state is a transition, defer endstr
            endstr = (endstr && sm_state(new_state)->event > 0);
            state = new_state;
        }
      next:
        ntransitions++;
        continue;
    };
    return NULL;
}
