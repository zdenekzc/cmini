#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* realloc(NULL, n) is malloc(n); cmini's TextAdd (asm/asmlist.c) starts from NULL */
int main(void) {
    char *p = realloc(NULL, 64);
    printf("REALLOC-NULL=%s\n", p ? "ok" : "null");
    if (p) { memcpy(p, "abc", 4); printf("REALLOC-COPY=%s\n", p); }
    char *q = realloc(p, 4096);
    printf("REALLOC-GROW=%s\n", q ? "ok" : "null");
    return 0;
}
