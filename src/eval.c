#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#ifndef WL_AMALGAMATION
#include "eval.h"
#endif

#define FRAME_LIMIT 128
#define EVAL_STACK_LIMIT 128
#define GROUP_LIMIT 128

#define HEAP_BASE 0xFEEDBEEFFEEDBEEF

/*
int
float
array
map
html
bool
none
*/

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

typedef struct {
    int group;
    int return_addr;
} Frame;

typedef struct {

    String code;
    String data;
    int off;

    Arena *a;

    char *errbuf;
    int   errmax;
    int   errlen;

    int num_frames;
    Frame frames[FRAME_LIMIT];

    int   eval_depth;
    Value eval_stack[EVAL_STACK_LIMIT];

    int num_groups;
    int groups[GROUP_LIMIT];

} Eval;

#define VALUE_NONE  ((Value) 0)
#define VALUE_TRUE  ((Value) 1)
#define VALUE_FALSE ((Value) 2)
#define VALUE_ERROR ((Value) 6)

void eval_report(Eval *e, char *fmt, ...)
{
    if (e->errmax == 0 || e->errlen > 0)
        return;

    int len = snprintf(e->errbuf, e->errmax, "Error: ");
    if (len < 0) {
        // TODO
    }

    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(e->errbuf + len, e->errmax - len, fmt, args);
    va_end(args);
    if (ret < 0) {
        // TODO
    }
    len += ret;

    e->errlen = len;
}

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

MapValue *get_map(Value v)
{
    return (MapValue*) (v & ~(uintptr_t) 7);
}

ArrayValue *get_array(Value v)
{
    return (ArrayValue*) (v & ~(uintptr_t) 7);
}

Value make_int(Eval *e, int64_t x)
{
    if (x <= (int64_t) (1ULL << 60)-1 && x >= (int64_t) -(1ULL << 60))
        return ((Value) x << 3) | 3;

    IntValue *v = alloc(e->a, (int) sizeof(IntValue), _Alignof(IntValue));
    if (v == NULL) {
        eval_report(e, "Out of memory");
        return VALUE_ERROR;
    }

    v->type = TYPE_INT;
    v->raw  = x;

    assert(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
}

Value make_float(Eval *e, float x)
{
    FloatValue *v = alloc(e->a, (int) sizeof(FloatValue), _Alignof(FloatValue));
    if (v == NULL) {
        eval_report(e, "Out of memory");
        return VALUE_ERROR;
    }

    v->type = TYPE_FLOAT;
    v->raw  = x;

    assert(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
}

Value make_str(Eval *e, String x) // TODO: This should reuse the string contents when possible
{
    StringValue *v = alloc(e->a, (int) sizeof(StringValue) + x.len, 8);
    if (v == NULL) {
        eval_report(e, "Out of memory");
        return VALUE_ERROR;
    }

    v->type = TYPE_STRING;
    v->len = x.len;
    memcpy(v->data, x.ptr, x.len);

    assert(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
}

Value make_map(Eval *e)
{
    MapValue *m = alloc(e->a, (int) sizeof(MapValue), _Alignof(MapValue));
    if (m == NULL) {
        eval_report(e, "Out of memory");
        return VALUE_ERROR;
    }

    m->type = TYPE_MAP;
    m->count = 0;
    m->tail_count = 0;
    m->tail = &m->head;

    return (Value) m | 7;
}

Value make_array(Eval *e)
{
    ArrayValue *a = alloc(e->a, (int) sizeof(ArrayValue), _Alignof(ArrayValue));
    if (a == NULL) {
        eval_report(e, "Out of memory");
        return VALUE_ERROR;
    }

    a->type = TYPE_ARRAY;
    a->count = 0;
    a->tail_count = 0;
    a->tail = &a->head;

    return (Value) a | 7;
}

b32 valeq(Value a, Value b);

int map_select(Eval *e, Value map, Value key, Value *val)
{
    (void) e;

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

int map_insert(Eval *e, Value map, Value key, Value val)
{
    MapValue *p = get_map(map);
    if (p->tail_count == ITEMS_PER_MAP_BATCH) {

        MapItems *batch = alloc(e->a, (int) sizeof(MapItems), _Alignof(MapItems));
        if (batch == NULL) {
            eval_report(e, "Out of memory");
            return -1;
        }

        batch->next = NULL;
        p->tail = batch;
        p->tail_count = 0;
    }

    p->tail->keys[p->tail_count] = key;
    p->tail->items[p->tail_count] = val;
    p->tail_count++;
    p->count++;
    return 0;
}

void value_print(Value v);

void map_print(Value v)
{
    printf("{ ");

    MapValue *p = get_map(v);
    MapItems *batch = &p->head;
    while (batch) {

        int num = ITEMS_PER_MAP_BATCH;
        if (batch->next == NULL)
            num = p->tail_count;

        for (int i = 0; i < num; i++) {
            value_print(batch->keys[i]);
            printf(": ");
            value_print(batch->items[i]);
            printf(", ");
        }

        batch = batch->next;
    }

    printf("}");
}

Value *array_select(Eval *e, Value array, int key)
{
    (void) e;

    ArrayValue *p = get_array(array);
    ArrayItems *batch = &p->head;
    int cursor = 0;
    while (batch) {

        int num = ITEMS_PER_MAP_BATCH;
        if (batch->next == NULL)
            num = p->tail_count;

        if (cursor <= key && key < cursor + num)
            return &batch->items[key - cursor];

        batch = batch->next;
        cursor += num;
    }

    return NULL;
}

int array_append(Eval *e, Value array, Value val)
{
    ArrayValue *p = get_array(array);
    if (p->tail_count == ITEMS_PER_MAP_BATCH) {

        ArrayItems *batch = alloc(e->a, (int) sizeof(ArrayItems), _Alignof(ArrayItems));
        if (batch == NULL) {
            eval_report(e, "Out of memory");
            return -1;
        }

        batch->next = NULL;
        p->tail = batch;
        p->tail_count = 0;
    }

    p->tail->items[p->tail_count] = val;
    p->tail_count++;
    p->count++;
    return 0;
}

void array_print(Value v)
{
    ArrayValue *p = get_array(v);
    ArrayItems *batch = &p->head;
    int cursor = 0;
    while (batch) {

        int num = ITEMS_PER_MAP_BATCH;
        if (batch->next == NULL)
            num = p->tail_count;

        for (int i = 0; i < num; i++)
            value_print(batch->items[i]);

        batch = batch->next;
        cursor += num;
    }
}

b32 valeq(Value a, Value b)
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

b32 valgrt(Value a, Value b)
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

void value_print(Value v)
{
    switch (type_of(v)) {

        case TYPE_NONE:
        printf("none");
        break;

        case TYPE_BOOL:
        printf(v == VALUE_TRUE ? "true" : "false");
        break;

        case TYPE_INT:
        printf("%" LLD, get_int(v));
        break;

        case TYPE_FLOAT:
        printf("%lf", get_float(v));
        break;

        case TYPE_MAP:
        map_print(v);
        break;

        case TYPE_ARRAY:
        array_print(v);
        break;

        case TYPE_STRING:
        {
            String s = get_str(v);
            printf("%.*s", s.len, s.ptr);
        }
        break;

        case TYPE_ERROR:
        printf("error");
        break;
    }
    fflush(stdout);
}

int step(Eval *e)
{
    uint8_t opcode = e->code.ptr[e->off];
/*
    printf("%-3d: ", e->off);
    print_instruction(e->code.ptr + e->off, e->data.ptr);
    printf("\n");
*/
    e->off++;

    switch (opcode) {

        case OPCODE_NOPE:
        {
            // Do nothing
        }
        break;

        case OPCODE_EXIT:
        {
            return 1;
        }
        break;

        case OPCODE_GROUP:
        {
            e->groups[e->num_groups++] = e->eval_depth;
        }
        break;

        case OPCODE_GPOP:
        {
            int group = e->groups[--e->num_groups];
            e->eval_depth = group;
        }
        break;

        case OPCODE_GPRINT:
        {
            for (int i = e->groups[e->num_groups-1]; i < e->eval_depth; i++)
                value_print(e->eval_stack[i]);
        }
        break;

        case OPCODE_GCOALESCE:
        {
            e->num_groups--;
        }
        break;

        case OPCODE_GTRUNC:
        {
            uint32_t num;
            memcpy(&num, (uint8_t*) e->code.ptr + e->off, sizeof(uint32_t));
            e->off += (int) sizeof(uint32_t);

            int group_size = e->eval_depth - e->groups[e->num_groups-1];

            if (group_size < (int) num) {
                for (int i = 0; i < (int) num - group_size; i++)
                    e->eval_stack[e->eval_depth + i] = VALUE_NONE;
            }

            e->eval_depth = e->groups[e->num_groups-1] + num;
        }
        break;

        case OPCODE_GOVERWRITE:
        {
            int current = e->groups[e->num_groups-1];
            int parent  = e->groups[e->num_groups-2];

            int current_size = e->eval_depth - current;

            for (int i = 0; i < current_size; i++)
                e->eval_stack[parent + i] = e->eval_stack[current + i];

            e->num_groups--;
            e->eval_depth = parent + current_size;
        }
        break;

        case OPCODE_GPACK:
        {
            Value array = make_array(e);
            if (array == VALUE_ERROR)
                return -1;
            for (int i = e->groups[e->num_groups-1]; i < e->eval_depth; i++)
                array_append(e, array, e->eval_stack[i]);

            e->eval_depth = e->groups[--e->num_groups];
            e->eval_stack[e->eval_depth++] = array;
        }
        break;

        case OPCODE_PUSHN:
        {
            e->eval_stack[e->eval_depth++] = VALUE_NONE;
        }
        break;

        case OPCODE_PUSHI:
        {
            int64_t x;
            memcpy(&x, (uint8_t*) e->code.ptr + e->off, sizeof(x));
            e->off += (int) sizeof(x);

            Value v = make_int(e, x);
            if (v == VALUE_ERROR) return -1;

            e->eval_stack[e->eval_depth++] = v;
        }
        break;

        case OPCODE_PUSHF:
        {
            double x;
            memcpy(&x, (uint8_t*) e->code.ptr + e->off, sizeof(x));
            e->off += (int) sizeof(x);

            Value v = make_float(e, x);
            if (v == VALUE_ERROR) return -1;

            e->eval_stack[e->eval_depth++] = v;
        }
        break;

        case OPCODE_PUSHS:
        {
            uint32_t off;
            memcpy(&off, (uint8_t*) e->code.ptr + e->off, sizeof(uint32_t));
            e->off += (int) sizeof(uint32_t);

            uint32_t len;
            memcpy(&len, (uint8_t*) e->code.ptr + e->off, sizeof(uint32_t));
            e->off += (int) sizeof(uint32_t);

            Value v = make_str(e, (String) { e->data.ptr + off, len });
            if (v == VALUE_ERROR) return -1;

            e->eval_stack[e->eval_depth++] = v;
        }
        break;

        case OPCODE_PUSHV:
        {
            uint8_t idx;
            memcpy(&idx, (uint8_t*) e->code.ptr + e->off, sizeof(uint8_t));
            e->off += sizeof(uint8_t);

            int group = e->frames[e->num_frames-1].group;
            Value v = e->eval_stack[e->groups[group] + idx];

            e->eval_stack[e->eval_depth++] = v;
        }
        break;

        case OPCODE_PUSHA:
        {
            uint32_t cap;
            memcpy(&cap, (uint8_t*) e->code.ptr + e->off, sizeof(uint32_t));
            e->off += sizeof(uint32_t);

            Value v = make_array(e);
            if (v == VALUE_ERROR) return -1;

            e->eval_stack[e->eval_depth++] = v;
        }
        break;

        case OPCODE_PUSHM:
        {
            uint32_t cap;
            memcpy(&cap, (uint8_t*) e->code.ptr + e->off, sizeof(uint32_t));
            e->off += sizeof(uint32_t);

            Value v = make_map(e);
            if (v == VALUE_ERROR) return -1;

            e->eval_stack[e->eval_depth++] = v;
        }
        break;

        case OPCODE_POP:
        {
            assert(e->num_groups == 0 || e->eval_depth > e->groups[e->num_groups-1]);
            e->eval_depth--;
        }
        break;

        case OPCODE_NEG:
        {
            Value a = e->eval_stack[--e->eval_depth];
            Type  t = type_of(a);

            Value r;
            if (0) {}
            else if (t == TYPE_INT)   r = make_int(e, -get_int(a));
            else if (t == TYPE_FLOAT) r = make_float(e, -get_float(a));
            else {
                eval_report(e, "Invalid operation on non-numeric value");
                return -1;
            }

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_EQL:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            Value r = valeq(a, b) ? VALUE_TRUE : VALUE_FALSE;
            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_NQL:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            Value r = valeq(a, b) ? VALUE_FALSE : VALUE_TRUE;
            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_LSS:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            if (type_of(a) != TYPE_INT || type_of(b) != TYPE_INT) {
                eval_report(e, "Invalid operation on non-numeric value");
                return -1;
            }

            Value r = valgrt(a, b) || valeq(a, b) ? VALUE_FALSE : VALUE_TRUE;
            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_GRT:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            if (type_of(a) != TYPE_INT || type_of(b) != TYPE_INT) {
                eval_report(e, "Invalid operation on non-numeric value");
                return -1;
            }

            Value r = valgrt(a, b) ? VALUE_TRUE : VALUE_FALSE;
            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_ADD:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            #define TYPE_PAIR(X, Y) (((uint16_t) (X) << 16) | (uint16_t) (Y))

            Type t1 = type_of(a);
            Type t2 = type_of(b);

            Value r;
            switch (TYPE_PAIR(t1, t2)) {

                case TYPE_PAIR(TYPE_INT, TYPE_INT):
                {
                    int64_t u = get_int(a);
                    int64_t v = get_int(b);
                    // TODO: check overflow and underflow
                    r = make_int(e, u + v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(e, u + v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(e, u + v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    // TODO: check overflow and underflow
                    r = make_float(e, u + v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                default:
                eval_report(e, "Invalid operation on non-numeric value");
                return -1;
            }

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_SUB:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            Type t1 = type_of(a);
            Type t2 = type_of(b);

            Value r;
            switch (TYPE_PAIR(t1, t2)) {

                case TYPE_PAIR(TYPE_INT, TYPE_INT):
                {
                    int64_t u = get_int(a);
                    int64_t v = get_int(b);
                    // TODO: check overflow and underflow
                    r = make_int(e, u - v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(e, u - v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(e, u - v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    // TODO: check overflow and underflow
                    r = make_float(e, u - v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                default:
                eval_report(e, "Invalid operation on non-numeric value");
                return -1;
            }

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_MUL:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            Type t1 = type_of(a);
            Type t2 = type_of(b);

            Value r;
            switch (TYPE_PAIR(t1, t2)) {

                case TYPE_PAIR(TYPE_INT, TYPE_INT):
                {
                    int64_t u = get_int(a);
                    int64_t v = get_int(b);
                    // TODO: check overflow and underflow
                    r = make_int(e, u * v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(e, u * v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(e, u * v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    // TODO: check overflow and underflow
                    r = make_float(e, u * v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                default:
                eval_report(e, "Invalid operation on non-numeric value");
                return -1;
            }

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_DIV:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            Type t1 = type_of(a);
            Type t2 = type_of(b);

            Value r;
            switch (TYPE_PAIR(t1, t2)) {

                case TYPE_PAIR(TYPE_INT, TYPE_INT):
                {
                    // TODO: check division by 0

                    int64_t u = get_int(a);
                    int64_t v = get_int(b);
                    r = make_int(e, u / v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    // TODO: check division by 0

                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(e, u / v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    // TODO: check division by 0

                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(e, u / v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    r = make_float(e, u / v);
                    if (r == VALUE_ERROR) return -1;
                }
                break;

                default:
                eval_report(e, "Invalid operation on non-numeric value");
                return -1;
            }

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_MOD:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            Type t1 = type_of(a);
            Type t2 = type_of(b);

            if (t1 != TYPE_INT || t2 != TYPE_INT) {
                eval_report(e, "Invalid modulo operation on non-integer value");
                return -1;
            }

            int64_t u = get_int(a);
            int64_t v = get_int(b);
            Value r = make_int(e, u % v);
            if (r == VALUE_ERROR) return -1;

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_SETV:
        {
            uint8_t x;
            memcpy(&x, (uint8_t*) e->code.ptr + e->off, (int) sizeof(x));
            e->off += (int) sizeof(x);

            Frame *f = &e->frames[e->num_frames-1];
            e->eval_stack[e->groups[f->group] + x] = e->eval_stack[--e->eval_depth];
        }
        break;

        case OPCODE_JUMP:
        {
            uint32_t x;
            memcpy(&x, (uint8_t*) e->code.ptr + e->off, (int) sizeof(x));
            e->off = x;
        }
        break;

        case OPCODE_JIFP:
        {
            uint32_t x;
            memcpy(&x, (uint8_t*) e->code.ptr + e->off, (int) sizeof(x));
            e->off += (int) sizeof(x);

            Value a = e->eval_stack[--e->eval_depth];

            if (a == VALUE_FALSE)
                e->off = x;
            else {
                if (a != VALUE_TRUE) {
                    eval_report(e, "Invalid operation on non-boolean value");
                    return -1;
                }
            }
        }
        break;

        case OPCODE_CALL:
        {
            uint32_t off;
            memcpy(&off, (uint8_t*) e->code.ptr + e->off, sizeof(uint32_t));
            e->off += (int) sizeof(uint32_t);

            if (e->num_frames == FRAME_LIMIT) {
                eval_report(e, "Frame limit reached");
                return -1;
            }
            e->frames[e->num_frames++] = (Frame) {.return_addr=e->off, .group=e->num_groups-1};

            e->off = off;
        }
        break;

        case OPCODE_RET:
        {
            e->off = e->frames[--e->num_frames].return_addr;
        }
        break;

        case OPCODE_APPEND:
        {
            Value val = e->eval_stack[e->eval_depth-1];
            Value set = e->eval_stack[e->eval_depth-2];
            e->eval_depth--;

            if (type_of(set) != TYPE_ARRAY) {
                eval_report(e, "Invalid operation on non-array value");
                return -1;
            }

            int ret = array_append(e, set, val);
            if (ret < 0) return -1;
        }
        break;

        case OPCODE_INSERT1:
        {
            Value key = e->eval_stack[e->eval_depth-1];
            Value val = e->eval_stack[e->eval_depth-2];
            Value set = e->eval_stack[e->eval_depth-3];
            e->eval_depth -= 2;

            if (type_of(set) == TYPE_ARRAY) {

                Value *dst = array_select(e, set, key);
                if (dst == NULL) {
                    eval_report(e, "Index out of range");
                    return -1;
                }
                *dst = val;

            } else if (type_of(set) == TYPE_MAP) {

                int ret = map_insert(e, set, key, val);
                if (ret < 0) return -1;

            } else {
                eval_report(e, "Invalid insertion on non-array and non-map value");
                return -1;
            }
        }
        break;

        case OPCODE_INSERT2:
        {
            Value key = e->eval_stack[e->eval_depth-1];
            Value set = e->eval_stack[e->eval_depth-2];
            Value val = e->eval_stack[e->eval_depth-3];
            e->eval_depth -= 2;

            if (type_of(set) == TYPE_ARRAY) {

                Value *dst = array_select(e, set, key);
                if (dst == NULL) {
                    eval_report(e, "Index out of range");
                    return -1;
                }
                *dst = val;

            } else if (type_of(set) == TYPE_MAP) {

                int ret = map_insert(e, set, key, val);
                if (ret < 0) return -1;

            } else {
                eval_report(e, "Invalid insertion on non-array and non-map value");
                return -1;
            }
        }
        break;

        case OPCODE_SELECT:
        {
            Value key = e->eval_stack[e->eval_depth-1];
            Value set = e->eval_stack[e->eval_depth-2];
            e->eval_depth -= 2;

            Value r;
            if (type_of(set) == TYPE_ARRAY) {

                Value *src = array_select(e, set, key);
                if (src == NULL) {
                    assert(0); // TODO
                }
                r = *src;

            } else if (type_of(set) == TYPE_MAP) {

                int ret = map_select(e, set, key, &r);
                if (ret < 0) {
                    assert(0); // TODO
                }

            } else {
                eval_report(e, "Invalid selection from non-array and non-map value");
                return -1;
            }

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_PRINT:
        {
            Value v = e->eval_stack[e->eval_depth-1];
            value_print(v);
        }
        break;

        default:
        eval_report(e, "Invalid opcode (offset %d)", e->off-1);
        return -1;
    }

    return 0;
}

int eval(Program p, Arena *a, char *errbuf, int errmax)
{
    String code;
    String data;

    int ret = parse_program_header(p, &code, &data, errbuf, errmax);
    if (ret < 0)
        return -1;

    Eval e = {
        .code=code,
        .data=data,
        .off=0,
        .a=a,
        .errbuf=errbuf,
        .errmax=errmax,
        .errlen=0,
        .num_frames=0,
        .eval_depth=0,
        .num_groups=0,
    };

    e.frames[e.num_frames++] = (Frame) { 0, 0 };

    for (;;) {
        int ret = step(&e);
        if (ret < 0) return -1;
        if (ret == 1) break;
    }

    e.num_frames--;
    return 0;
}