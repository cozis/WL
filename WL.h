#ifndef WL_AMALGAMATION
#define WL_AMALGAMATION

// This file was generated automatically. Do not modify directly!

////////////////////////////////////////////////////////////////////////////////////////
// src/public.h
////////////////////////////////////////////////////////////////////////////////////////

#line 1 "src/public.h"
#ifndef WL_PUBLIC_INCLUDED
#define WL_PUBLIC_INCLUDED

#ifndef WL_AMALGAMATION
#include "includes.h"
#endif

typedef struct WL_State WL_State;

typedef struct {
    char *ptr;
    int   len;
} WL_String;

typedef struct {
    char *ptr;
    int   len;
} WL_Program;

typedef enum {
    WL_DONE,
    WL_ERROR,
    WL_VAR,
    WL_CALL,
    WL_OUTPUT,
} WL_ResultType;

typedef struct {
    WL_ResultType type;
    WL_String     str;
} WL_Result;

typedef struct {
    char *ptr;
    int   len;
    int   cur;
} WL_Arena;

int       WL_compile    (char *src, int len, WL_Arena arena, WL_Program *program, char *err, int errmax);
WL_State *WL_State_init (WL_Arena *a, WL_Program p, char *err, int errmax);
void      WL_State_free (WL_State *state);
WL_Result WL_eval       (WL_State *state);
int       WL_streq      (WL_String a, char *b, int blen);
int       WL_popint     (WL_State *state, long long *x);
int       WL_popfloat   (WL_State *state, float *x);
int       WL_popstr     (WL_State *state, char **str, int *len);
int       WL_popany     (WL_State *state);
void      WL_select     (WL_State *state);
void      WL_pushint    (WL_State *state, long long x);
void      WL_pushfloat  (WL_State *state, float x);
void      WL_pushstr    (WL_State *state, char *str, int len);
void      WL_pusharray  (WL_State *state, int cap);
void      WL_pushmap    (WL_State *state, int cap);
void      WL_insert     (WL_State *state);

#endif // WL_PUBLIC_INCLUDED
#endif // WL_AMALGAMATION
