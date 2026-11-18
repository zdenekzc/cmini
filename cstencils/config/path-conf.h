
/* path-conf.h */

#ifndef __PROG_PATH_CONF_H__
#define __PROG_PATH_CONF_H__

// directories

#define BIN_DIR            "/abc/gibbon"
#define INCLUDE_DIR        "/abc/gibbon"
#define LIB_DIR            "/abc/gibbon"
#define DATA_DIR           "/abc/gibbon"

// setup files

#define SETUP_FILES        ""
#define SETUP_VARIABLE     ""

// boolean values

#define LOCAL_DATA_PARAM   true
#define LOCAL_RPATH_PARAM  true

// values from configure

#define PTHREAD_CFLAGS     ""
#define PTHREAD_LIBS       "-lpthread"

#define TRACE_CFLAGS       "-DTRACE"
#define TRACE_LIBS         ""

#define DEMANGLE_CFLAGS    "-DDEMANGLE"
#define DEMANGLE_LIBS      ""

#define BFD_CFLAGS         "-DBFD"
#define BFD_LIBS           "-lbfd -liberty"

#define LTDL_CFLAGS        "-DLTDL"
#define LTDL_LIBS          "-rdynamic -lltdl"

#define DL_CFLAGS          ""
#define DL_LIBS            ""

#endif // __PROG_PATH_CONF_H__
