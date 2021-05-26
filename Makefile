# GNU Makefile for RegExTest (ret)

# required to support strlcpy on Linux
ifeq (${shell uname -s},Linux)
LDLIBS += -lbsd
endif

.PHONY: test test-gold clean

CFLAGS = -g

TARGETS = ret.o re.o sm.o dq.o

ret: ${TARGETS}

ret.o: ret.c re.h

re.o: re.c re.h sm.h dq.h

sm.o: sm.c sm.h

dq.o: dq.c dq.h

clean:
	rm -rf ret ${TARGETS} test/test.results

test:
	sh test/test.sh >test/test.results
	diff -u test/test.gold test/test.results

test-gold:
	sh test/test.sh >test/test.gold
