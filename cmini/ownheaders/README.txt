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

Missing system includes are silent by default. To check that nothing is
skipped, set CMINI_MISSING_HEADERS=report (the names go to stderr) or =error
(the compilation fails). The tests compile the examples, cstencils and cmini
itself with =error, so a new <...> include without a replacement is caught.

Usage:  CMINI_MISSING_HEADERS=error cmini/cmini.bin -I cmini/ownheaders file.c
        cmini/test_instr.bin -0 -I cmini/ownheaders file.c

stdbool.h - bool, true and false are built into c-mini (a keyword); the file
only provides the standard macros for a real C compiler.

iostream - std::cout, std::cerr and std::endl are built in (cmini/expr.c) and
the translator writes printf / dprintf. The file declares nothing and exists
only so that #include <iostream> is not silently ignored.

containers.h - templates Sequence <T> and Dictionary <K, V> for g++ (cparser
has them built in, the content is under #ifndef __CPARSER__); the -p output
inserts it as #include "containers.h" - compile g++ with -iquote
cmini/ownheaders (not -I cmini/ownheaders: the other headers in this
directory are only for cparser).

unistd.h, sys/wait.h, sys/stat.h, cstdint, stdint.h - for ../cmini/instr.cc
(CHANGELOG.txt in cparser, chapter 25): processes, descriptors, waitpid and the
macros WIFEXITED ..., chmod, fixed-width types.

ctime, time.h - time, clock (tests/examples/quicksort.cc, heapsort.c:
srand (time (NULL))).
