/* statdump.c - fstat of a file with the cmini header of ToaruOS: prints the raw words of
 * struct stat and the fields used by mini-commander (checks the layout against the kernel) */
#include "cmini_libc.h"
#include <fcntl.h>
#include <sys/stat.h>

int main(void)
{
    stat_t st;
    puts("SD-START");
    unsigned long *w = (unsigned long *) &st;
    int fd = open("/tmp/mp/a.txt", O_RDONLY);
    int i;
    puts("SD-OPENED");
    if (fd < 0) { printf("open failed\n"); return 1; }
    for (i = 0; i < 14; i++) w[i] = 0x5A5A5A5A5A5A5A5AUL;
    if (fstat(fd, &st) != 0) { printf("fstat failed\n"); return 1; }
    for (i = 0; i < 14; i++) printf("W%d=%lx\n", i, w[i]);
    printf("SIZE=%ld MODE=%o MTIME=%ld\n", (long) st.st_size, (unsigned) st.st_mode, (long) st.st_mtime);
    close(fd);
    return 0;
}
