/* hello.c - the smoke test of the aarch64 cross TinyCC (mktcc-aarch64.sh): a static ET_EXEC */
#include <stdio.h>

int main (void)
{
   printf ("Hello from TinyCC on ToaruOS aarch64\n");
   return 0;
}
