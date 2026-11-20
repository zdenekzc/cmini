#ifndef CMINI_GETOPT_H_GUARD
#define CMINI_GETOPT_H_GUARD
/* getopt.h - cmini declarations (glibc) */
#define no_argument 0
#define required_argument 1
#define optional_argument 2
struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};
extern char *optarg;
extern int optind;
int getopt_long (int argc, char *const *argv, const char *shortopts,
                 const struct option *longopts, int *longind);

#endif
