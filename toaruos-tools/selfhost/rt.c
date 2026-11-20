#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char gbig[200000];          /* zero-initialised global (.bss), as in cmini's tables */
static void *gptrs[4096];
/* statically initialised pointer tables: need data relocations (cmini tables in instr.c, pasm) */
static const char *names[] = { "alpha", "beta", "gamma" };
static int one(int x) { return x + 1; }
static int two(int x) { return x + 2; }
static int (*ops[])(int) = { one, two };
static int cmp(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
int main(void) {
    int i; char *p = NULL; char *s; char buf[64]; FILE *f; int arr[5] = {5, 3, 9, 1, 7};
    int nz = 0;
    printf("RT-START\n");
    for (i = 0; i < 200000; i++) if (gbig[i]) nz++;
    for (i = 0; i < 4096; i++) if (gptrs[i]) nz++;
    printf("BSS-NONZERO=%d\n", nz);
    p = malloc(16);
    for (i = 16; i < 70000; i *= 2) { p = realloc(p, i); if (!p) { printf("REALLOC-FAIL %d\n", i); return 1; } memset(p, 1, i); }
    printf("REALLOC-OK\n");
    free(p);
    s = strdup("hello strdup");
    printf("STRDUP=%s\n", s); free(s);
    snprintf(buf, sizeof(buf), "%d-%s-%ld", 42, "x", 123456789L);
    printf("SNPRINTF=%s\n", buf);
    qsort(arr, 5, sizeof(int), cmp);
    printf("QSORT=%d %d %d %d %d\n", arr[0], arr[1], arr[2], arr[3], arr[4]);
    f = fopen("/tmp/rt-data.txt", "w"); if (f) { fprintf(f, "line one\n"); fclose(f); }
    f = fopen("/tmp/rt-data.txt", "r");
    if (f) { if (fgets(buf, sizeof(buf), f)) printf("FGETS=%s", buf); fclose(f); } else printf("FOPEN-FAIL\n");
    printf("TABLE=%s %s %s\n", names[0], names[1], names[2]);
    printf("FNTABLE=%d %d\n", ops[0](10), ops[1](10));
    printf("RT-END\n");
    return 0;
}
