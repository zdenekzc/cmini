#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* TextAdd of asm/asmlist.c, copied: realloc from NULL, then memcpy with an int size */
typedef struct { char *text; int len; int cap; } TEXT;
static void TextAdd (TEXT * t, const char * s)
{
   int n = (int) strlen (s);
   if (t->len + n + 1 > t->cap)
   {
      t->cap = 2 * (t->len + n + 1) + 64;
      t->text = (char *) realloc (t->text, t->cap);
      if (t->text == NULL) abort ();
   }
   printf ("TX-BEFORE-MEMCPY dst=%p n=%d\n", (void *) (t->text + t->len), n);
   memcpy (t->text + t->len, s, n + 1);
   printf ("TX-AFTER-MEMCPY\n");
   t->len += n;
}
int main(void) {
    TEXT t = { NULL, 0, 0 };
    const char *s = "; vy..... a string of length 67 ............................................";
    TextAdd(&t, s);
    printf("TX-RESULT=%s\n", t.text);
    return 0;
}
