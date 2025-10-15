#include <stdint.h>
#include <stdbool.h>

#define WL_STR(X) ((WL_String) { (X), (int) sizeof(X)-1 })

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

// Creates a compilation unit for a program
// The provided arena (which can't be NULL) is
// used for all memory allocations until a
// program is produced or an error occurs.
// If not enough memory is provided, NULL is
// returned.
WL_Compiler *wl_compiler_init(WL_Arena *arena);

// Adds a file to the current compilation unit
// and returns
//
//   WL_ADD_ERROR if an error occurred
//
//   WL_ADD_AGAIN if the file included a different
//   file that also needs to be added to the unit,
//   in which case the file name is in the "path"
//   field of the return value.
//
//   WL_ADD_LINK all sources were processed and
//   the unit is ready for linking
//
// Note that the source of all files needs to
// stay alive until the program is linked as the
// compiler may keep references to it until then.
WL_AddResult wl_compiler_add(WL_Compiler *compiler, WL_String path, WL_String content);

// Links a compilation unit producing an executable.
// The output program is just an array of bytes
// allocated using the compiler's arena memory and
// may be written to disk or any other external system
// for caching.
//
// On error, -1 is returned and the error text can
// be retrieved using wl_compiler_error. On success,
// 0 is returned.
int wl_compiler_link(WL_Compiler *compiler, WL_Program *program);

// Returns the null-terminated error string for a
// compilation unit that failed.
WL_String wl_compiler_error(WL_Compiler *compiler);

// Serializes the AST of the source parsed in this
// compilation unit by writing the string to the
// rovided buffer and returning the number of bytes
// written to it.
//
// If the provided buffer is too small, the function
// fills it up and returns the number of bytes that
// would have been written if the buffer was large
// enough. On error, -1 is returned.
int wl_dump_ast(WL_Compiler *compiler, char *dst, int cap);

// Writes the bytecode of a program to stdout as a
// human-readable string.
void wl_dump_program(WL_Program program);

// Creates an evaluation context for a bytecode program
// All memory used while running the program will be
// allocated from the provided arena.
//
// If not enough memory was provided or the program is
// invalid, NULL is returned.
WL_Runtime *wl_runtime_init(WL_Arena *arena, WL_Program program);

// Run the program associated to this runtime until an
// event happens. The event may be one of:
//
//   WL_EVAL_DONE if execution is complete
//
//   WL_EVAL_ERROR if execution failed
//
//   WL_EVAL_OUTPUT if data is available for output,
//   in which case the field "str" points to it
//
//   WL_EVAL_SYSVAR if the program requested the value
//   of an external symbol.
//
//   WL_EVAL_SYSCALL
//
WL_EvalResult wl_runtime_eval(WL_Runtime *rt);

WL_String     wl_runtime_error(WL_Runtime *rt);

void          wl_runtime_dump(WL_Runtime *rt);

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
