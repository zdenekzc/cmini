#include <stdio.h>
#include <setjmp.h>
/* pasm calls setjmp on every line in its normal path (asm/inst.c, asm/code.c):
 * setjmp must return 0 at the first call, and longjmp must return the value. */
static jmp_buf buf;
static int depth(int n) {
    if (setjmp(buf) == 0) {
        if (n > 0) return depth(n - 1);
        longjmp(buf, 7);
    }
    return n + 100;
}
int main(void) {
    int first = setjmp(buf);
    printf("JMP-FIRST=%d\n", first);
    if (first == 0) {
        int r = depth(2);
        printf("JMP-DEPTH=%d\n", r);
    }
    return 0;
}
