#ifndef WL_BASIC_INCLUDED
#define WL_BASIC_INCLUDED

typedef unsigned int b32;
#define true 1
#define false 0

typedef struct {
    char *ptr;
    int   len;
} String;

typedef struct {
    char *ptr;
    int   len;
    int   cur;
} Arena;

#ifdef _WIN32
#define LLU "llu"
#else
#define LLU "lu"
#endif

#define S(X) (String) { (X), (int) sizeof(X)-1 }

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

b32  is_space(char c);
b32  is_digit(char c);
b32  is_alpha(char c);
b32  is_printable(char c);
char to_lower(char c);
b32  is_hex_digit(char c);
int  hex_digit_to_int(char c);

b32 streq(String a, String b);
b32 streqcase(String a, String b);

void *alloc(Arena *a, int len, int align);
b32 grow_alloc(Arena *a, char *p, int new_len);

#endif // WL_BASIC_INCLUDED