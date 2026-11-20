#ifndef TINY_TOARU_DIRENT_H_GUARD
#define TINY_TOARU_DIRENT_H_GUARD
/* dirent.h for ToaruOS (cmini build): the ToaruOS layout, which has no d_type.
 * The plain readdir of ToaruOS reads the old layout (names come out empty),
 * so readdir is redirected to readdir64, as in mini-commander/cmini-toaru. */
typedef struct dirent {
    unsigned long d_ino;
    char d_name[256];
} dirent_t;
typedef struct __dirstream DIR;
DIR *opendir (const char *name);
int closedir (DIR *dir);
dirent_t *readdir64 (DIR *dir);
#define readdir readdir64

#endif
