#define _XOPEN_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <math.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>

#include "common.h"

double get_time (void)
{
    struct timeval t;
    gettimeofday (& t, 0);
    double timestamp = t.tv_sec + (double) t.tv_usec / 1000000;
    return timestamp;
}

