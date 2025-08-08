
#ifndef WL_AMALGAMATION
#include "value.h"
#endif

#define ITEMS_PER_MAP_BATCH 8
#define ITEMS_PER_ARRAY_BATCH 16

typedef struct MapItems MapItems;
struct MapItems {
    MapItems *next;
    Value     keys [ITEMS_PER_MAP_BATCH];
    Value     items[ITEMS_PER_MAP_BATCH];
};

typedef struct {
    Type      type;
    int       count;
    int       tail_count;
    MapItems  head;
    MapItems *tail;
} MapValue;

typedef struct ArrayItems ArrayItems;
struct ArrayItems {
    ArrayItems *next;
    Value       items[ITEMS_PER_ARRAY_BATCH];
};

typedef struct {
    Type        type;
    int         count;
    int         tail_count;
    ArrayItems  head;
    ArrayItems *tail;
} ArrayValue;

typedef struct {
    Type   type;
    double raw;
} FloatValue;

typedef struct {
    Type    type;
    int64_t raw;
} IntValue;

typedef struct {
    Type type;
    int  len;
    char data[];
} StringValue;

Type type_of(Value v)
{
    // 000 none
    // 001 true
    // 010 false
    // 011 int
    // 100
    // 101
    // 110 error
    // 111 pointer

    switch (v & 7) {
        case 0: return TYPE_NONE;
        case 1: return TYPE_BOOL;
        case 2: return TYPE_BOOL;
        case 3: return TYPE_INT;
        case 4: break;
        case 5: break;
        case 6: return TYPE_ERROR;
        case 7: return *(Type*) ((uintptr_t) v & ~(uintptr_t) 7);
    }

    return TYPE_ERROR;
}

int64_t get_int(Value v)
{
    if ((v & 7) == 3)
        return (int64_t) (v >> 3);

    IntValue *p = (IntValue*) v;
    return p->raw;
}

float get_float(Value v)
{
    FloatValue *p = (FloatValue*) v;
    return p->raw;
}

String get_str(Value v)
{
    StringValue *p = (StringValue*) (v & ~(uintptr_t) 7);
    return (String) { p->data, p->len };
}

static MapValue *get_map(Value v)
{
    return (MapValue*) (v & ~(uintptr_t) 7);
}

static ArrayValue *get_array(Value v)
{
    return (ArrayValue*) (v & ~(uintptr_t) 7);
}

Value make_int(WL_Arena *a, int64_t x)
{
    if (x <= (int64_t) (1ULL << 60)-1 && x >= (int64_t) -(1ULL << 60))
        return ((Value) x << 3) | 3;

    IntValue *v = alloc(a, (int) sizeof(IntValue), _Alignof(IntValue));
    if (v == NULL)
        return VALUE_ERROR;

    v->type = TYPE_INT;
    v->raw  = x;

    assert(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
}

Value make_float(WL_Arena *a, float x)
{
    FloatValue *v = alloc(a, (int) sizeof(FloatValue), _Alignof(FloatValue));
    if (v == NULL)
        return VALUE_ERROR;

    v->type = TYPE_FLOAT;
    v->raw  = x;

    assert(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
}

Value make_str(WL_Arena *a, String x) // TODO: This should reuse the string contents when possible
{
    StringValue *v = alloc(a, (int) sizeof(StringValue) + x.len, 8);
    if (v == NULL)
        return VALUE_ERROR;

    v->type = TYPE_STRING;
    v->len = x.len;
    memcpy(v->data, x.ptr, x.len);

    assert(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
}

Value make_map(WL_Arena *a)
{
    MapValue *m = alloc(a, (int) sizeof(MapValue), _Alignof(MapValue));
    if (m == NULL)
        return VALUE_ERROR;

    m->type = TYPE_MAP;
    m->count = 0;
    m->tail_count = 0;
    m->tail = &m->head;
    m->head.next = NULL;

    return (Value) m | 7;
}

Value make_array(WL_Arena *a)
{
    ArrayValue *v = alloc(a, (int) sizeof(ArrayValue), _Alignof(ArrayValue));
    if (v == NULL)
        return VALUE_ERROR;

    v->type = TYPE_ARRAY;
    v->count = 0;
    v->tail_count = 0;
    v->tail = &v->head;
    v->head.next = NULL;

    return (Value) v | 7;
}

int map_select(Value map, Value key, Value *val)
{
    MapValue *p = get_map(map);
    MapItems *batch = &p->head;
    while (batch) {

        int num = ITEMS_PER_MAP_BATCH;
        if (batch->next == NULL)
            num = p->tail_count;

        for (int i = 0; i < num; i++)
            if (valeq(batch->keys[i], key)) {
                *val = batch->items[i];
                return 0;
            }

        batch = batch->next;
    }

    return -1;
}

Value *map_select_by_index(Value map, int key)
{
    MapValue *p = get_map(map);
    MapItems *batch = &p->head;
    int cursor = 0;
    while (batch) {

        int num = ITEMS_PER_MAP_BATCH;
        if (batch->next == NULL)
            num = p->tail_count;

        if (cursor <= key && key < cursor + num)
            return &batch->keys[key - cursor];

        batch = batch->next;
        cursor += num;
    }

    return NULL;
}

int map_insert(WL_Arena *a, Value map, Value key, Value val)
{
    MapValue *p = get_map(map);
    if (p->tail_count == ITEMS_PER_MAP_BATCH) {

        MapItems *batch = alloc(a, (int) sizeof(MapItems), _Alignof(MapItems));
        if (batch == NULL)
            return -1;

        batch->next = NULL;
        if (p->tail)
            p->tail->next = batch;
        p->tail = batch;
        p->tail_count = 0;
    }

    p->tail->keys[p->tail_count] = key;
    p->tail->items[p->tail_count] = val;
    p->tail_count++;
    p->count++;
    return 0;
}

Value *array_select(Value array, int key)
{
    ArrayValue *p = get_array(array);
    ArrayItems *batch = &p->head;
    int cursor = 0;
    while (batch) {

        int num = ITEMS_PER_ARRAY_BATCH;
        if (batch->next == NULL)
            num = p->tail_count;

        if (cursor <= key && key < cursor + num)
            return &batch->items[key - cursor];

        batch = batch->next;
        cursor += num;
    }

    return NULL;
}

int array_append(WL_Arena *a, Value array, Value val)
{
    ArrayValue *p = get_array(array);
    if (p->tail_count == ITEMS_PER_ARRAY_BATCH) {

        ArrayItems *batch = alloc(a, (int) sizeof(ArrayItems), _Alignof(ArrayItems));
        if (batch == NULL)
            return -1;

        batch->next = NULL;

        if (p->tail)
            p->tail->next = batch;
        p->tail = batch;
        p->tail_count = 0;
    }

    p->tail->items[p->tail_count] = val;
    p->tail_count++;
    p->count++;
    return 0;
}

bool valeq(Value a, Value b)
{
    Type t1 = type_of(a);
    Type t2 = type_of(b);

    if (t1 != t2)
        return false;

    switch (t1) {

        case TYPE_NONE:
        return VALUE_TRUE;

        case TYPE_BOOL:
        return a == b;

        case TYPE_INT:
        return get_int(a) == get_int(b);

        case TYPE_FLOAT:
        return get_float(a) == get_float(b);

        case TYPE_MAP:
        return false; // TODO

        case TYPE_ARRAY:
        return false; // TODO

        case TYPE_STRING:
        return streq(get_str(a), get_str(b));

        case TYPE_ERROR:
        return true;
    }

    return false;
}

bool valgrt(Value a, Value b)
{
    Type t1 = type_of(a);
    Type t2 = type_of(b);

    if (t1 != t2)
        return false;

    switch (t1) {

        case TYPE_NONE:
        return VALUE_FALSE;

        case TYPE_BOOL:
        return VALUE_FALSE;

        case TYPE_INT:
        return get_int(a) > get_int(b);

        case TYPE_FLOAT:
        return get_float(a) > get_float(b);

        case TYPE_MAP:
        return false;

        case TYPE_ARRAY:
        return false;

        case TYPE_STRING:
        return false;

        case TYPE_ERROR:
        return false;
    }

    return false;
}

int value_length(Value v)
{
    Type type = type_of(v);

    if (type == TYPE_ARRAY)
        return get_array(v)->count;

    if (type == TYPE_MAP)
        return get_map(v)->count;

    return -1;
}

typedef struct {
    char *dst;
    int   max;
    int   len;
} ToStringContext;

static void tostr_appends(ToStringContext *tostr, String x)
{
    if (tostr->max > tostr->len) {
        int cpy = tostr->max - tostr->len;
        if (cpy > x.len)
            cpy = x.len;
        memcpy(tostr->dst + tostr->len, x.ptr, cpy);
    }
    tostr->len += x.len;
}

static void tostr_appendi(ToStringContext *tostr, int64_t x)
{
    int len;
    if (tostr->max >= tostr->len)
        len = snprintf(tostr->dst + tostr->len, tostr->max - tostr->len, "%" LLD, x);
    else
        len = snprintf(NULL, 0, "%" LLD, x);
    tostr->len += len;
}

static void tostr_appendf(ToStringContext *tostr, double x)
{
    int len;
    if (tostr->max >= tostr->len)
        len = snprintf(tostr->dst + tostr->len, tostr->max - tostr->len, "%f", x);
    else
        len = snprintf(NULL, 0, "%f", x);
    tostr->len += len;
}

static void value_to_string_inner(Value v, ToStringContext *tostr)
{
    switch (type_of(v)) {

        case TYPE_NONE:
        //tostr_appends(tostr, S("none"));
        break;

        case TYPE_BOOL:
        // TODO
        //tostr_appends(tostr, get_bool(v) ? S("true") : S("false"));
        break;

        case TYPE_INT:
        tostr_appendi(tostr, get_int(v));
        break;

        case TYPE_FLOAT:
        tostr_appendf(tostr, get_float(v));
        break;

        case TYPE_MAP:
        {
            tostr_appends(tostr, S("{ "));
            MapValue *m = get_map(v);
            MapItems *batch = &m->head;
            while (batch) {

                int num = ITEMS_PER_MAP_BATCH;
                if (batch->next == NULL)
                    num = m->tail_count;

                for (int i = 0; i < num; i++) {
                    value_to_string_inner(batch->keys[i], tostr);
                    tostr_appends(tostr, S(": "));
                    value_to_string_inner(batch->items[i], tostr);
                    
                    if (batch->next != NULL || i+1 < num)
                        tostr_appends(tostr, S(", "));
                }

                batch = batch->next;
            }
            tostr_appends(tostr, S(" }"));
        }
        break;

        case TYPE_ARRAY:
        {
            ArrayValue *a = get_array(v);
            ArrayItems *batch = &a->head;
            int cursor = 0;
            while (batch) {

                int num = ITEMS_PER_ARRAY_BATCH;
                if (batch->next == NULL)
                    num = a->tail_count;

                for (int i = 0; i < num; i++)
                    value_to_string_inner(batch->items[i], tostr);

                batch = batch->next;
                cursor += num;
            }
        }
        break;

        case TYPE_STRING:
        tostr_appends(tostr, get_str(v));
        break;

        case TYPE_ERROR:
        tostr_appends(tostr, S("error"));
        break;
    }
}

int value_to_string(Value v, char *dst, int max)
{
    ToStringContext tostr = { dst, max, 0 };
    value_to_string_inner(v, &tostr);
    return tostr.len;
}