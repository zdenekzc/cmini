#include <stdio.h>
/* large local arrays as in cmini's instr.c (char buf [65536]) */
static int deep(int n) {
    char buf[65536];
    buf[n % 65536] = (char)n;
    if (n <= 0) return buf[0];
    return deep(n - 1) + (buf[n % 65536] != 0);
}
int main(void) {
    char big[200000];
    big[199999] = 5;
    printf("STACK-BIG=%d\n", big[199999]);
    printf("STACK-DEEP=%d\n", deep(3));
    return 0;
}
