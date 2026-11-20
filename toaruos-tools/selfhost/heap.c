#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* heap stress with the allocation pattern of cmini (many small calloc/strdup, growing
 * realloc buffers, frees in between); every block is filled and checked */
#define N 4000
static char *keep[N];
static unsigned long seed = 12345;
static unsigned long rnd(void) { seed = seed * 1103515245UL + 12345UL; return (seed >> 8) & 0xFFFFFF; }
int main(void) {
    int i, k, bad = 0;
    unsigned long total = 0;
    for (i = 0; i < N; i++) {
        unsigned long size = 16 + rnd() % 300;
        char *p = calloc(1, size);
        if (!p) { printf("HEAP-NULL at %d\n", i); return 1; }
        memset(p, (char)i, size - 1);
        p[size - 1] = 0;
        keep[i] = p;
        total += size;
        if (i % 7 == 3) {                      /* free an older block, as cmini frees temporaries */
            int j = (int)(rnd() % (i + 1));
            if (keep[j]) { free(keep[j]); keep[j] = NULL; }
        }
        if (i % 50 == 0) {                     /* growing buffer like TextAdd */
            char *b = NULL; unsigned long cap = 0, len = 0;
            for (k = 0; k < 40; k++) {
                unsigned long n = 1 + rnd() % 80;
                if (len + n + 1 > cap) { cap = 2 * (len + n + 1) + 64; b = realloc(b, cap); if (!b) { printf("REALLOC-NULL\n"); return 1; } }
                memset(b + len, 'x', n); len += n; b[len] = 0;
            }
            if (strlen(b) != len) bad++;
            free(b);
        }
    }
    for (i = 0; i < N; i++) {
        if (keep[i]) {
            unsigned long size = strlen(keep[i]) + 1;
            if (size > 1 && keep[i][0] != (char)i && keep[i][0] != 0) bad++;
        }
    }
    printf("HEAP-DONE total=%lu bad=%d\n", total, bad);
    return bad != 0;
}
