#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#endif

#ifndef WL_AMALGAMATION
#include "file.h"
#endif

int file_open(String path, File *handle, int *size)
{
    char zt[1<<10];
    if (path.len >= COUNT(zt))
        return -1;
    memcpy(zt, path.ptr, path.len);
    zt[path.len] = '\0';

#ifdef _WIN32
    *handle = CreateFileA(
        zt,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (*handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND ||
            error == ERROR_ACCESS_DENIED)
            return 1;
        return -1;
    }
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(*handle, &fileSize)) {
        CloseHandle(*handle);
        return -1;
    }
    if (fileSize.QuadPart > INT_MAX) {
        CloseHandle(*handle);
        return -1;
    }
    *size = (int) fileSize.QuadPart;
#else
    *handle = open(zt, O_RDONLY);
    if (*handle < 0) {
        if (errno == ENOENT)
            return 1;
        return -1;
    }
    struct stat info;
    if (fstat(*handle, &info) < 0) {
        close(*handle);
        return -1;
    }
    if (S_ISDIR(info.st_mode)) {
        close(*handle);
        return 1;
    }
    if (info.st_size > INT_MAX) {
        close(*handle);
        return -1;
    }
    *size = (int) info.st_size;
#endif
    return 0;
}

void file_close(File file)
{
#ifdef _WIN32
	CloseHandle(file);
#else
	close(file);
#endif
}

int file_read(File file, char *dst, int max)
{
#ifdef _WIN32
    DWORD num;
    BOOL ok = ReadFile(file, dst, max, &num, NULL);
    if (!ok)
        return -1;
    return (int) num;
#else
    return read(file, dst, max);
#endif
}

int file_read_all(String path, String *dst)
{
    int len;
    File handle;
    if (file_open(path, &handle, &len) < 0)
        return -1;

    char *ptr = malloc(len+1);
    if (ptr == NULL) {
        file_close(handle);
        return -1;
    }

    for (int copied = 0; copied < len; ) {
        int ret = file_read(handle, ptr + copied, len - copied);
        if (ret <= 0) {
            free(ptr);
            file_close(handle);
            return -1;
        }
        copied += ret;
    }

    *dst = (String) { ptr, len };
    file_close(handle);
    return 0;
}
