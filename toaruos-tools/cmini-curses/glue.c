/* glue.c - symbols that cmini calls and the ToaruOS libc does not provide under that name,
 * or does not provide at all (the functions fail with ENOSYS, as mc then reports an error). */
extern int errno;

int *__errno_location(void)
{
    return &errno;
}

/* fsync: the ToaruOS guest does not sync; saves stay as durable as rename() is */
int fsync(int fd)
{
    (void) fd;
    return 0;
}

int mkfifo(const char *path, unsigned int mode)
{
    (void) path;
    (void) mode;
    errno = 38;     /* ENOSYS */
    return -1;
}

int mknod(const char *path, unsigned int mode, unsigned long dev)
{
    (void) path;
    (void) mode;
    (void) dev;
    errno = 38;
    return -1;
}

int utimensat(int dirfd, const char *path, const void *times, int flags)
{
    (void) dirfd;
    (void) path;
    (void) times;
    (void) flags;
    errno = 38;
    return -1;
}
