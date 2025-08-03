#include <stddef.h>
#include <stdint.h>

#ifndef WL_AMALGAMATION
#include "basic.h"
#endif

b32 is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

b32 is_digit(char c)
{
    return c >= '0' && c <= '9';
}

b32 is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

b32 is_printable(char c)
{
    return c >= ' ' && c <= '~';
}

b32 is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

char to_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return c;
}

int hex_digit_to_int(char c)
{
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return c - '0';
}


b32 streq(String a, String b)
{
    if (a.len != b.len)
        return false;
    for (int i = 0; i < a.len; i++)
        if (a.ptr[i] != b.ptr[i])
            return false;
    return true;
}

b32 streqcase(String a, String b)
{
    if (a.len != b.len)
        return false;
    for (int i = 0; i < a.len; i++)
        if (to_lower(a.ptr[i]) != to_lower(b.ptr[i]))
            return false;
    return true;
}

void *alloc(Arena *a, int len, int align)
{
    int pad = -(intptr_t) (a->ptr + a->cur) & (align-1);
    if (a->len - a->cur < len + pad)
        return NULL;
    void *ret = a->ptr + a->cur + pad;
    a->cur += pad + len;
    return ret;
}

b32 grow_alloc(Arena *a, char *p, int new_len)
{
    int new_cur = (p - a->ptr) + new_len;
    if (new_cur > a->len)
        return false;
    a->cur = new_cur;
    return true;
}