#ifndef CMINI_SYS_WAIT_H_GUARD
#define CMINI_SYS_WAIT_H_GUARD
/* sys/wait.h - cparser (glibc) */

#ifndef CPARSER_SYS_WAIT
#define CPARSER_SYS_WAIT

#include <unistd.h>

pid_t waitpid (pid_t pid, int * status, int options);

/* stav procesu (jako glibc): bity 0 - 6 signal, 8 - 15 navratovy kod */
#define WTERMSIG(s)     ((s) & 0x7f)
#define WEXITSTATUS(s)  (((s) & 0xff00) >> 8)
#define WIFEXITED(s)    (WTERMSIG (s) == 0)
#define WIFSIGNALED(s)  (WTERMSIG (s) != 0 && WTERMSIG (s) != 0x7f)

#endif
#define WNOHANG 1

#endif
