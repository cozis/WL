
#ifndef WL_AMALGAMATION
#include "includes.h"
#include "basic.h"
#include "public.h"
#endif

bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_printable(char c)
{
    return c >= ' ' && c <= '~';
}

bool is_hex_digit(char c)
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


bool streq(String a, String b)
{
    if (a.len != b.len)
        return false;
    for (int i = 0; i < a.len; i++)
        if (a.ptr[i] != b.ptr[i])
            return false;
    return true;
}

bool streqcase(String a, String b)
{
    if (a.len != b.len)
        return false;
    for (int i = 0; i < a.len; i++)
        if (to_lower(a.ptr[i]) != to_lower(b.ptr[i]))
            return false;
    return true;
}

void *alloc(WL_Arena *a, int len, int align)
{
    int pad = -(intptr_t) (a->ptr + a->cur) & (align-1);
    if (a->len - a->cur < len + pad)
        return NULL;
    void *ret = a->ptr + a->cur + pad;
    a->cur += pad + len;
    return ret;
}

bool grow_alloc(WL_Arena *a, char *p, int new_len)
{
    int new_cur = (p - a->ptr) + new_len;
    if (new_cur > a->len)
        return false;
    a->cur = new_cur;
    return true;
}

String copystr(String s, WL_Arena *a)
{
    char *p = alloc(a, s.len, 1);
    if (p == NULL)
        return (String) { NULL, 0 };
    memcpy(p, s.ptr, s.len);
    return (String) { p, s.len };
}