/* preproc.h - vkladany soubor pro tests/examples/preproc.cc */

#ifndef PREPROC_H
#define PREPROC_H

#define SIZE 10

#define SQUARE(x) ((x) * (x))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define SHOW(expr) printf ("%s = %d\n", #expr, (expr))

#endif /* PREPROC_H */
