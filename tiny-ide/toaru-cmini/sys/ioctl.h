/* sys/ioctl.h - cmini declarations (ToaruOS) */
#ifndef TOARU_CMINI_SYS_IOCTL_H
#define TOARU_CMINI_SYS_IOCTL_H
#include "../termios.h"
int ioctl(int fd, unsigned long request, void *arg);
#endif
