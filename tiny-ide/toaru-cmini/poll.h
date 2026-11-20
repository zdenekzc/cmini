/* poll.h - cmini declarations (ToaruOS) */
#ifndef TOARU_CMINI_POLL_H
#define TOARU_CMINI_POLL_H
typedef unsigned int nfds_t;
struct pollfd {
    int fd;
    short events;
    short revents;
};
#define POLLIN 0x0001
typedef struct pollfd pollfd_t;
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
#endif
