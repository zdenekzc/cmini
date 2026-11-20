#include <stdio.h>
/* a local struct with a brace initializer must set every field (asm/asmlist.c AsmPrintNode:
 * TEXT t = { NULL, 0, 0 }; the text is then passed to realloc) */
typedef struct { char *text; int len; int cap; } TEXTX;
static int noise(void) { volatile int junk[64]; int i; for (i = 0; i < 64; i++) junk[i] = -1; return junk[3]; }
static void fill(void) { TEXTX z; z.text = (char *) 12345; z.len = 99; z.cap = 77; (void) z; }
int main(void) {
    TEXTX t = { NULL, 0, 0 };
    fill();
    noise();
    {
        TEXTX u = { NULL, 0, 0 };
        printf("INIT-COMPOUND text=%p len=%d cap=%d\n", (void *) t.text, t.len, t.cap);
        printf("INIT-LOCAL text=%p len=%d cap=%d\n", (void *) u.text, u.len, u.cap);
    }
    return 0;
}
