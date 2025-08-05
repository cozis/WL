#ifndef WL_BASIC_INCLUDED
#define WL_BASIC_INCLUDED

#ifndef WL_AMALGAMATION
#include "public.h"
#endif

typedef struct {
    char *ptr;
    int   len;
} String;

#ifdef _WIN32
#define LLU "llu"
#define LLD "lld"
#else
#define LLU "lu"
#define LLD "ld"
#endif

#define S(X) (String) { (X), (int) sizeof(X)-1 }

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

#define COUNT(X) (int) (sizeof(X) / sizeof((X)[0]))

bool is_space(char c);
bool is_digit(char c);
bool is_alpha(char c);
bool is_printable(char c);
char to_lower(char c);
bool is_hex_digit(char c);
int  hex_digit_to_int(char c);

bool streq(String a, String b);
bool streqcase(String a, String b);

void *alloc(WL_Arena *a, int len, int align);
bool grow_alloc(WL_Arena *a, char *p, int new_len);

#endif // WL_BASIC_INCLUDED