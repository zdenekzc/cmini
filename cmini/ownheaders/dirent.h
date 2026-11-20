#ifndef CMINI_DIRENT_H_GUARD
#define CMINI_DIRENT_H_GUARD
/* dirent.h - cmini declarations (layout of glibc x86_64 struct dirent) */
#define DT_DIR 4
#define DT_REG 8
struct dirent {
    unsigned long d_ino;
    long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};
typedef struct dirent dirent_t;
typedef struct __dirstream DIR;
DIR *opendir (const char *name);
dirent_t *readdir (DIR *dir);
int closedir (DIR *dir);

#endif
