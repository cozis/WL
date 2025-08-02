#ifndef WL_FILE_INCLUDED
#define WL_FILE_INCLUDED

#include "basic.h"

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HANDLE File;
#else
typedef int File;
#endif

int  file_open(String path, File *handle, int *size);
void file_close(File file);
int  file_read(File file, char *dst, int max);
int  file_read_all(String path, String *dst);

#endif // WL_FILE_INCLUDED