#include <stdint.h>

typedef struct WL_Runtime  WL_Runtime;
typedef struct WL_Compiler WL_Compiler;

typedef struct {
    char *ptr;
    int   len;
} WL_String;

typedef struct {
    char *ptr;
    int   len;
    int   cur;
} WL_Arena;

typedef struct {
    char *ptr;
    int   len;
} WL_Program;

typedef enum {
    WL_ADD_ERROR,
    WL_ADD_AGAIN,
    WL_ADD_LINK,
} WL_AddResultType;

typedef struct {
    WL_AddResultType type;
    WL_String        path;
} WL_AddResult;

typedef enum {
    WL_EVAL_NONE,
    WL_EVAL_DONE,
    WL_EVAL_ERROR,
    WL_EVAL_OUTPUT,
    WL_EVAL_SYSVAR,
    WL_EVAL_SYSCALL,
} WL_EvalResultType;

typedef struct {
    WL_EvalResultType type;
    WL_String str;
} WL_EvalResult;

WL_Compiler*  wl_compiler_init  (WL_Arena *arena);
WL_AddResult  wl_compiler_add   (WL_Compiler *compiler, WL_String content);
int           wl_compiler_link  (WL_Compiler *compiler, WL_Program *program);
WL_String     wl_compiler_error (WL_Compiler *compiler);
int           wl_dump_ast       (WL_Compiler *compiler, char *dst, int cap);
void          wl_dump_program   (WL_Program program);

WL_Runtime*   wl_runtime_init   (WL_Arena *arena, WL_Program program);
WL_EvalResult wl_runtime_eval   (WL_Runtime *rt);
WL_String     wl_runtime_error  (WL_Runtime *rt);
void          wl_runtime_dump   (WL_Runtime *rt);

bool wl_streq      (WL_String a, char *b, int blen);
int  wl_arg_count  (WL_Runtime *rt);
bool wl_arg_none   (WL_Runtime *rt, int idx);
bool wl_arg_bool   (WL_Runtime *rt, int idx, bool *x);
bool wl_arg_s64    (WL_Runtime *rt, int idx, int64_t *x);
bool wl_arg_f64    (WL_Runtime *rt, int idx, double *x);
bool wl_arg_str    (WL_Runtime *rt, int idx, WL_String *x);
bool wl_arg_array  (WL_Runtime *rt, int idx);
bool wl_arg_map    (WL_Runtime *rt, int idx);
bool wl_peek_none  (WL_Runtime *rt, int off);
bool wl_peek_bool  (WL_Runtime *rt, int off, bool *x);
bool wl_peek_s64   (WL_Runtime *rt, int off, int64_t *x);
bool wl_peek_f64   (WL_Runtime *rt, int off, double *x);
bool wl_peek_str   (WL_Runtime *rt, int off, WL_String *x);
bool wl_pop_any    (WL_Runtime *rt);
bool wl_pop_none   (WL_Runtime *rt);
bool wl_pop_bool   (WL_Runtime *rt, bool *x);
bool wl_pop_s64    (WL_Runtime *rt, int64_t *x);
bool wl_pop_f64    (WL_Runtime *rt, double *x);
bool wl_pop_str    (WL_Runtime *rt, WL_String *x);
void wl_push_none  (WL_Runtime *rt);
void wl_push_true  (WL_Runtime *rt);
void wl_push_false (WL_Runtime *rt);
void wl_push_s64   (WL_Runtime *rt, int64_t x);
void wl_push_f64   (WL_Runtime *rt, double x);
void wl_push_str   (WL_Runtime *rt, WL_String x);
void wl_push_array (WL_Runtime *rt, int cap);
void wl_push_map   (WL_Runtime *rt, int cap);
void wl_push_arg   (WL_Runtime *rt, int idx);
void wl_insert     (WL_Runtime *rt);
void wl_append     (WL_Runtime *rt);
