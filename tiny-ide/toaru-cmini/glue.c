/* glue.c - symbols the ToaruOS libc does not export under the names cmini calls.
 * errno: libc has the variable errno; cmini's ownheaders errno.h calls __errno_location. */
extern int errno;

int *__errno_location(void)
{
    return &errno;
}

/* fsync: the ToaruOS guest does not sync; saves stay as durable as rename() */
int fsync(int fd)
{
    (void) fd;
    return 0;
}
