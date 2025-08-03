#include <string.h>

#ifndef WL_AMALGAMATION
#include "eval.h"
#endif

#define FRAME_LIMIT 128
#define EVAL_STACK_LIMIT 128

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

typedef struct MapItems MapItems;
struct MapItems {
    MapItems *next;
    Value     keys[8];
    Value     items[8];
};

typedef struct {
    Type      type;
    int       count;
    MapItems  head;
    MapItems *tail;
} MapValue;

typedef struct ArrayItems ArrayItems;
struct ArrayItems {
    ArrayItems *next;
    Value       items[16];
};

typedef struct {
    Type        type;
    int         count;
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
} Frame;

typedef struct {

    Program p;
    int off;

    Arena *a;

    int frame_depth;
    Frame frames[FRAME_LIMIT];

    int   eval_depth;
    Value eval_stack[EVAL_STACK_LIMIT];

} Eval;

#define VALUE_NONE  ((Value) 0)
#define VALUE_TRUE  ((Value) 1)
#define VALUE_FALSE ((Value) 2)
#define VALUE_ERROR ((Value) 6)

Type type_of(Value v)
{
    // 000 none
    // 001 true
    // 010 false
    // 011 int
    // 100 string
    // 101
    // 110 error
    // 111 pointer

    switch (v & 7) {
        case 0: return TYPE_NONE;
        case 1: return TYPE_BOOL;
        case 2: return TYPE_BOOL;
        case 3: return TYPE_INT;
        case 4: return TYPE_STRING;
        case 5: break;
        case 6: return TYPE_ERROR;
        case 7: return *(Type*) v;
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

String get_str(Value *v)
{
    if ((*v & 7) == 6)
        return (String) { (char*) v, strnlen((char*) v, 7) };

    StringValue *p = (StringValue*) v;
    return (String) { p->data, p->len };
}

Value make_int(Eval *e, int64_t x)
{
    if (x <= (1ULL << 60)-1 && x >= -(1ULL << 60))
        return ((Value) x >> 3) | 3;

    IntValue *v = alloc(e->a, (int) sizeof(IntValue), _Alignof(IntValue));
    if (v == NULL)
        return VALUE_ERROR;

    v->type = TYPE_INT;
    v->raw  = x;

    ASSERT(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
}

Value make_float(Eval *e, float x)
{
    FloatValue *v = alloc(e->a, (int) sizeof(FloatValue), _Alignof(FloatValue));
    if (v == NULL)
        return VALUE_ERROR;

    v->type = TYPE_FLOAT;
    v->raw  = x;

    ASSERT(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
}

Value make_str(Eval *e, String x)
{
    if (x.len < 8) {
        Value v = 0;
        memcpy(&v, x.ptr, x.len);
        return v | 4;
    }

    StringValue *v = alloc(e->a, (int) sizeof(StringValue) + x.len, _Alignof(StringValue));
    if (v == NULL)
        return VALUE_ERROR;

    v->len = x.len;
    memcpy(v->data, x.ptr, x.len);

    ASSERT(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
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

void step(Eval *e)
{
    uint8_t b = ((uint8_t*) e->p.ptr)[e->off++];

    switch (b) {

        case OPCODE_NOPE:
        // Do nothing
        break;

        case OPCODE_PUSHI:
        {
            int64_t x;

            memcpy(&x, (uint8_t*) e->p.ptr + e->off, sizeof(x));
            e->off += (int) sizeof(x);

            e->eval_stack[e->eval_depth++] = x;
        }
        break;

        case OPCODE_PUSHF:
        {
            double x;

            memcpy(&x, (uint8_t*) e->p.ptr + e->off, sizeof(x));
            e->off += (int) sizeof(x);

            e->eval_stack[e->eval_depth++] = x;
        }
        break;

        case OPCODE_PUSHS:
        {
            // TODO
        }
        break;

        case OPCODE_PUSHV:
        {
            // TODO
        }
        break;

        case OPCODE_PUSHA:
        {
            // TODO
        }
        break;

        case OPCODE_PUSHM:
        {
            // TODO
        }
        break;

        case OPCODE_POP:
        e->eval_depth--;
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
                // TODO
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

            Value r = valgrt(a, b) || valeq(a, b) ? VALUE_FALSE : VALUE_TRUE;
            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_GRT:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

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
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(e, u + v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(e, u + v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    // TODO: check overflow and underflow
                    r = make_float(e, u + v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                default:
                // TODO
                break;
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
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(e, u - v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(e, u - v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    // TODO: check overflow and underflow
                    r = make_float(e, u - v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                default:
                // TODO
                break;
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
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(e, u * v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(e, u * v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    // TODO: check overflow and underflow
                    r = make_float(e, u * v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                default:
                // TODO
                break;
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
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    // TODO: check division by 0

                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(e, u / v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    // TODO: check division by 0

                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(e, u / v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    r = make_float(e, u / v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                default:
                // TODO
                break;
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

            Value r;
            switch (TYPE_PAIR(t1, t2)) {

                case TYPE_PAIR(TYPE_INT, TYPE_INT):
                {
                    // TODO: check division by 0

                    int64_t u = get_int(a);
                    int64_t v = get_int(b);
                    r = make_int(e, u % v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    // TODO: check division by 0

                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(e, u % v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    // TODO: check division by 0

                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(e, u % v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    r = make_float(e, u % v);
                    if (r == VALUE_ERROR) {
                        // TODO
                    }
                }
                break;

                default:
                // TODO
                break;
            }

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_SETV:
        {
            // TODO
        }
        break;

        case OPCODE_JUMP:
        {
            uint32_t x;

            memcpy(&x, (uint8_t*) e->p.ptr + e->off, (int) sizeof(x));
            e->off = (int) sizeof(x);
        }
        break;

        case OPCODE_JIFP:
        {
            // TODO
        }
        break;

        case OPCODE_CALL:
        {
            // TODO
        }
        break;

        case OPCODE_RET:
        {
            // TODO
        }
        break;

        case OPCODE_APPEND:
        {
            Value val = e->eval_stack[e->eval_depth-1];
            Value set = e->eval_stack[e->eval_depth-2];
            e->eval_depth--;

            // TODO
        }
        break;

        case OPCODE_INSERT1:
        {
            Value key = e->eval_stack[e->eval_depth-1];
            Value val = e->eval_stack[e->eval_depth-2];
            Value set = e->eval_stack[e->eval_depth-3];
            e->eval_depth -= 2;

            // TODO
        }
        break;

        case OPCODE_INSERT2:
        {
            Value key = e->eval_stack[e->eval_depth-1];
            Value set = e->eval_stack[e->eval_depth-2];
            Value val = e->eval_stack[e->eval_depth-3];
            e->eval_depth -= 2;

            // TODO
        }
        break;

        case OPCODE_SELECT:
        {
            // TODO
        }
        break;

        default:
        break;
    }
}

void eval(Program p)
{
    Eval e = { p, 0 };
    for (;;)
        step(&e);

    // TODO
}