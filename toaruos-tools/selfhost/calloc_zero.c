/* calloc for ToaruOS: its libc calloc does not zero the memory (the kernel does not
 * zero new pages from sbrk; measured in cecko toaruos-tools README), and cmini relies
 * on zeroed memory.  The compiler objects call it as cm_calloc (cc -D calloc=cm_calloc),
 * so libc.a calloc is not pulled in twice. */
#include <stdlib.h>
#include <string.h>

void *cm_calloc(size_t count, size_t size)
{
    size_t total = count * size;
    void *p = malloc(total ? total : 1);
    if (p)
        memset(p, 0, total);
    return p;
}
