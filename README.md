# re

Regular expression matcher, written in C.  Based on the algorithm
described in Chapter 20 of "Algorithms" by Robert Sedgewick (1984).

## SYNOPSIS

```C
#include "re.h"

struct sm_fsm*
re_compile(char* re_str, int flags);

struct re_matched*
re_match(struct sm_fsm* fsm, char* search_str);
```


## DESCRIPTION

The re_compile function compiles the regular expression passed as
re_str. The int flags parameter is currently unused, but required.
The function returns a pointer to the compiled regex.

The re_match function is passed the compiled regex pointer, as fsm,
and a string to search, search_str.  If a match is found, a pointer to
an re_matched structure is returned.  NULL is returned for no match.

The re_match structure is:
```C
 struct re_matched {
    int start;
    int end;
  };
```

where start is the character position within search_str where the
match starts and end is the last character position of the match.

## NOTES

An example of using the above API can be found in `ret.c`.

The following regex special characters are supported:

```
*    zero or more occurances of the previous character
|    alternate
( )  grouping
^    start of string anchor
$    end of string anchor
.    any character
[ ]  character class. Ranges indicated by -. ^ as first char negates
```

Special characters are escaped by a backslash '\\'.
