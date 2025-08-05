#ifndef WL_FILE_INCLUDED
#define WL_FILE_INCLUDED

#ifndef WL_AMALGAMATION
#include "includes.h"
#include "basic.h"
#endif

#ifdef _WIN32
typedef HANDLE File;
#else
typedef int File;
#endif

int  file_open(String path, File *handle, int *size);
void file_close(File file);
int  file_read(File file, char *dst, int max);
int  file_read_all(String path, String *dst);

#endif // WL_FILE_INCLUDED