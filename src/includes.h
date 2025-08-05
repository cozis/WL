#ifndef WL_INCLUDES_INCLUDED
#define WL_INCLUDES_INCLUDED

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef _WIN32
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#endif

#endif // WL_INCLUDES_INCLUDED