/* cmini-compat.h - declarations that translated cmini programs use (tcc -include) */
#ifndef CMINI_COMPAT_H
#define CMINI_COMPAT_H
extern int *__errno_location (void);   /* errno as cmini writes it (ownheaders/errno.h) */
#endif
