
/* load.h */

/* dynamic loading */

#ifndef __PROG_LOAD_H__
#define __PROG_LOAD_H__

#include "std.h"

OPEN_NAMESPACE

string load_and_run (string file_name, string func_name);
// file_name - dynamic library
// func_name - function identifier
// result - empty string or error description

CLOSE_NAMESPACE

#endif /* __PROG_LOAD_H__ */
