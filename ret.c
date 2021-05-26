/* Regex test harness */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#ifdef linux
#include <bsd/string.h>
#endif

#include "re.h"

enum {
    BUFSIZE = 81,
};

int
main(int argc, char* argv[])
{
    char search[BUFSIZE], match_str[BUFSIZE];
    char* s;
    char* program = argv[0];
    struct re_matched* matched;
    struct sm_fsm* fsm;
    bool do_match = true;
    int error_code, re_compile_flags = RE_OPT;

    while (--argc > 0 && (*++argv)[0] == '-') {
        for (s = argv[0]+1; *s != '\0'; s++) {
            switch (*s) {
                case 'v':
                    debug = true;
                    break;
                case 'n':
                    do_match = false;
                    break;
                case 'o':
                    re_compile_flags = 0;
                    break;
                default:
                    fprintf(stderr,"%s: unknown switch: -%c\n",program,*s);
                    return EXIT_FAILURE;
            }
        }
    }
    if (argc > 0) {
        fsm = re_compile(argv[0],re_compile_flags);
        if (fsm == NULL) {
            fprintf(stderr,"ret: %s\n",re_error_msg());
            return EXIT_FAILURE;
        }
        if (debug) sm_print(fsm);
        if (!do_match) return EXIT_SUCCESS;

        while (fgets(search,BUFSIZE,stdin)) {
            if ((s = strrchr(search,'\n'))) *s = '\0';
            if ((matched = re_match(fsm, search))) {
                strlcpy(match_str, search+matched->start,
                        matched->end - matched->start + 1);
                printf("Found: %s\n",match_str);
            }
            else if (re_error_code != 0) {
                fprintf(stderr,"%s: %s\n",program, re_error_msg());
            }
        }
    }
    else {
        fprintf(stderr,"%s: regex required\n",program);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
