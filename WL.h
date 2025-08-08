#ifndef WL_AMALGAMATION
#define WL_AMALGAMATION

// This file was generated automatically. Do not modify directly!

////////////////////////////////////////////////////////////////////////////////////////
// src/compile.h
////////////////////////////////////////////////////////////////////////////////////////

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

typedef struct WL_Compiler WL_Compiler;

typedef enum {
    WL_COMPILE_RESULT_DONE,
    WL_COMPILE_RESULT_FILE,
    WL_COMPILE_RESULT_ERROR,
} WL_CompileResultType;

typedef struct {
    WL_CompileResultType type;
    WL_Program program;
    WL_String  path;
} WL_CompileResult;

WL_Compiler*     WL_Compiler_init (WL_Arena *arena);
void             WL_Compiler_free (WL_Compiler *compiler);
WL_CompileResult WL_compile       (WL_Compiler *compiler, WL_String file, WL_String content);
WL_State*        WL_State_init    (WL_Arena *a, WL_Program p, char *err, int errmax);
void             WL_State_free    (WL_State *state);
WL_Result        WL_eval          (WL_State *state);

int       WL_streq      (WL_String a, char *b, int blen);
int       WL_peeknone   (WL_State *state, int off);
int       WL_peekint    (WL_State *state, int off, long long *x);
int       WL_peekfloat  (WL_State *state, int off, float *x);
int       WL_peekstr    (WL_State *state, int off, WL_String *str);
int       WL_popnone    (WL_State *state);
int       WL_popint     (WL_State *state, long long *x);
int       WL_popfloat   (WL_State *state, float *x);
int       WL_popstr     (WL_State *state, WL_String *str);
int       WL_popany     (WL_State *state);
void      WL_select     (WL_State *state);
void      WL_pushnone   (WL_State *state);
void      WL_pushint    (WL_State *state, long long x);
void      WL_pushfloat  (WL_State *state, float x);
void      WL_pushstr    (WL_State *state, WL_String str);
void      WL_pusharray  (WL_State *state, int cap);
void      WL_pushmap    (WL_State *state, int cap);
void      WL_insert     (WL_State *state);
void      WL_append     (WL_State *state);

#endif // WL_PUBLIC_INCLUDED
#endif // WL_AMALGAMATION
