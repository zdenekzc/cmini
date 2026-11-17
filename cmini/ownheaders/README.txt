System headers for cparser (lexer.c, switch -I)
===============================================

Location: the directory was originally include/ in the project root; it was
moved here (cmini/ownheaders/).
Only cparser reads these headers; g++, chibicc and TinyCC get the path via
the switches -I / -iquote.

The cparser preprocessor ignores system headers it cannot find (<stdio.h>,
<climits>, ...). Here are their simplified replacements: constants (macros)
and function prototypes from glibc - without prototypes a function would get
the implicit type int f (...) and a result of pointer or long type would be
truncated to 32 bits.

Usage:  cmini/test_instr.bin -0 -I cmini/ownheaders file.c

containers.h - templates Sequence <T> and Dictionary <K, V> for g++ (cparser
has them built in, the content is under #ifndef __CPARSER__); the -p output
inserts it as #include "containers.h" - compile g++ with -iquote
cmini/ownheaders (not -I cmini/ownheaders: the other headers in this
directory are only for cparser).

unistd.h, sys/wait.h, sys/stat.h, cstdint, stdint.h - for ../cmini/instr.cc
(README.txt in cparser, chapter 25): processes, descriptors, waitpid and the
macros WIFEXITED ..., chmod, fixed-width types.

ctime, time.h - time, clock (tests/examples/quicksort.cc, heapsort.c:
srand (time (NULL))).
