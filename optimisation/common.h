#ifndef _COMMON_H_
#define _COMMON_H_

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <float.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#include <string.h>

typedef unsigned int uint;
typedef unsigned long int ulong;

enum { SUCCESS=100, ERROR, INVALID, CLOSED_PIPE, TIMEOUT, READY, DATA_ERROR, BAD_TYPE, EMPTY, RESET, UNINITIALIZED, TRUNCATED, };

#ifndef STDFS
#define STDFS stderr
#endif

#define LOG_LEVEL_DEBUG   5
#define LOG_LEVEL_STATUS  4
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_NONE    0

#ifndef logLevel
#define logLevel LOG_LEVEL_DEBUG
#endif

#define MESSAGE(fs,lvl,prefix,...)  do{if(lvl<=logLevel) {fprintf (fs, "%s:%d: %s: %s",__FILE__,__LINE__,__func__,prefix);fprintf (fs,__VA_ARGS__);fprintf(fs, "\n");fflush(fs);}}while(0)
#define MESSAGEn(fs,lvl,prefix,...) do{if(lvl<=logLevel) {fprintf (fs, "%s:%d: %s: %s",__FILE__,__LINE__,__func__,prefix);fprintf (fs,__VA_ARGS__);                  fflush(fs);}}while(0)

#define fprint_debug(fs,...)   MESSAGE  (fs, 5, "debug: ",   __VA_ARGS__)
#define fprint_status(fs,...)  MESSAGE  (fs, 4, "status: ",  __VA_ARGS__)
#define fprint_info(fs,...)    MESSAGE  (fs, 3, "info: ",    __VA_ARGS__)
#define fprint_warning(fs,...) MESSAGE  (fs, 2, "warning: ", __VA_ARGS__)
#define fprint_error(fs,...)   MESSAGE  (fs, 1, "error: ",   __VA_ARGS__)
#define fprint_abort(fs,...)   MESSAGE  (fs, 1, "abort: ",   __VA_ARGS__)
#define fprint(fs,...)         MESSAGE  (fs, 0, "",  __VA_ARGS__)
#define fprintn(fs,...)        MESSAGEn (fs, 0, "",  __VA_ARGS__)

#define fexit_debug(fs,...)    do {fprint_debug   (fs, __VA_ARGS__); exit (ERROR); } while (0)
#define fexit_status(fs,...)   do {fprint_status  (fs, __VA_ARGS__); exit (ERROR); } while (0)
#define fexit_info(fs,...)     do {fprint_info    (fs, __VA_ARGS__); exit (ERROR); } while (0)
#define fexit_warning(fs,...)  do {fprint_warning (fs, __VA_ARGS__); exit (ERROR); } while (0)
#define fexit_error(fs,...)    do {fprint_error   (fs, __VA_ARGS__); exit (ERROR); } while (0)
#define fexit_abort(fs,...)    do {fprint_abort   (fs, __VA_ARGS__); exit (ERROR); } while (0)

#define print_debug(...)       fprint_debug   (STDFS, __VA_ARGS__)
#define print_status(...)      fprint_status  (STDFS, __VA_ARGS__)
#define print_info(...)        fprint_info    (STDFS, __VA_ARGS__)
#define print_warning(...)     fprint_warning (STDFS, __VA_ARGS__)
#define print_error(...)       fprint_error   (STDFS, __VA_ARGS__)
#define print(...)             fprint         (STDFS, __VA_ARGS__)
#define printn(...)            fprintn        (STDFS, __VA_ARGS__)

#define exit_debug(...)        fexit_debug    (STDFS, __VA_ARGS__)
#define exit_error(...)        fexit_error    (STDFS, __VA_ARGS__)
#define exit_abort(...)        fexit_abort    (STDFS, __VA_ARGS__)
#define exit_warning(...)      fexit_warning  (STDFS, __VA_ARGS__)
#define exit_status(...)       fexit_status   (STDFS, __VA_ARGS__)
#define exit_info(...)         fexit_info     (STDFS, __VA_ARGS__)

#define fprintf_exit(fs,...) do {fprintf(fs,__VA_ARGS__); fprintf(fs,"\n"); exit(ERROR); } while (0)
#define printf_exit(...)  fprintf_exit (STDFS,__VA_ARGS__)

#define assert_verbose(test,...) if(!(test))exit_error(__VA_ARGS__)

#define equal(foo,bar) (strcmp(foo,bar)==0)
#define catch_nan(x) if (isnan(x)){exit_error ("caught nan! %s", #x);}
#define sign(x) ((x > 0) - (x < 0))

double get_time (void);

#ifdef __cplusplus
} /* end extern C */
#endif

#endif /* _COMMON_H_ */
