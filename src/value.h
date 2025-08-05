#ifndef WL_VALUE_INCLUDED
#define WL_VALUE_INCLUDED

#ifndef WL_AMALGAMATION
#include "basic.h"
#include "includes.h"
#endif

#define VALUE_NONE  ((Value) 0)
#define VALUE_TRUE  ((Value) 1)
#define VALUE_FALSE ((Value) 2)
#define VALUE_ERROR ((Value) 6)

typedef enum {
    TYPE_NONE,
    TYPE_BOOL,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_MAP,
    TYPE_ARRAY,
    TYPE_STRING,
    TYPE_ERROR,
} Type;

typedef uint64_t Value;

Type    type_of      (Value v);
int64_t get_int      (Value v);
float   get_float    (Value v);
String  get_str      (Value v);
Value   make_int     (WL_Arena *a, int64_t x);
Value   make_float   (WL_Arena *a, float x);
Value   make_str     (WL_Arena *a, String x);
Value   make_map     (WL_Arena *a);
Value   make_array   (WL_Arena *a);
int     map_select   (Value map, Value key, Value *val);
Value*  map_select_by_index(Value map, int key);
int     map_insert   (WL_Arena *a, Value map, Value key, Value val);
Value*  array_select (Value array, int key);
int     array_append (WL_Arena *a, Value array, Value val);
bool    valeq        (Value a, Value b);
bool    valgrt       (Value a, Value b);
int     value_length (Value v);
int     value_to_string(Value v, char *dst, int max);

#endif // WL_VALUE_INCLUDED