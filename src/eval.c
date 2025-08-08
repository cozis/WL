
#ifndef WL_AMALGAMATION
#include "includes.h"
#include "value.h"
#include "eval.h"
#endif

#define FRAME_LIMIT 128
#define EVAL_STACK_LIMIT 128
#define GROUP_LIMIT 128

typedef struct {
    int group;
    int return_addr;
} Frame;

struct WL_State {

    String code;
    String data;
    int off;

    bool trace;

    WL_Arena *a;

    char *errbuf;
    int   errmax;
    int   errlen;

    int num_frames;
    Frame frames[FRAME_LIMIT];

    int   eval_depth;
    Value eval_stack[EVAL_STACK_LIMIT];

    int num_groups;
    int groups[GROUP_LIMIT];

    int cur_print;
    int num_prints;

    String sysvar;
    String syscall;
    bool syscall_error;
    int stack_before_user;
    int stack_base_for_user;
};

void eval_report(WL_State *state, char *fmt, ...)
{
    if (state->errmax == 0 || state->errlen > 0)
        return;

    int len = snprintf(state->errbuf, state->errmax, "Error: ");
    if (len < 0) {
        // TODO
    }

    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(state->errbuf + len, state->errmax - len, fmt, args);
    va_end(args);
    if (ret < 0) {
        // TODO
    }
    len += ret;

    state->errlen = len;
}

static uint8_t read_u8(WL_State *state)
{
    assert(state->off >= 0);
    assert(state->off < state->code.len);
    return state->code.ptr[state->off++];
}

static void read_mem(WL_State *state, void *dst, int len)
{
    memcpy(dst, (uint8_t*) state->code.ptr + state->off, len);
    state->off += len;
}

static uint32_t read_u32(WL_State *state)
{
    uint32_t x;
    read_mem(state, &x, (int) sizeof(x));
    return x;
}

static int64_t read_s64(WL_State *state)
{
    int64_t x;
    read_mem(state, &x, (int) sizeof(x));
    return x;
}

static double read_f64(WL_State *state)
{
    double x;
    read_mem(state, &x, (int) sizeof(x));
    return x;
}

int step(WL_State *state)
{
    uint8_t opcode = read_u8(state);

    if (state->trace) {
        printf("%-3d: ", state->off-1);
        print_instruction(state->code.ptr + state->off - 1, state->data.ptr);
        printf("\n");
    }

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
            state->groups[state->num_groups++] = state->eval_depth;
        }
        break;

        case OPCODE_GPOP:
        {
            int group = state->groups[--state->num_groups];
            state->eval_depth = group;
        }
        break;

        case OPCODE_GPRINT:
        {
            state->num_prints = state->eval_depth - state->groups[state->num_groups-1];
        }
        break;

        case OPCODE_GCOALESCE:
        {
            state->num_groups--;
        }
        break;

        case OPCODE_GTRUNC:
        {
            uint32_t num = read_u32(state);

            int group_size = state->eval_depth - state->groups[state->num_groups-1];

            if (group_size < (int) num)
                for (int i = 0; i < (int) num - group_size; i++)
                    state->eval_stack[state->eval_depth + i] = VALUE_NONE;

            state->eval_depth = state->groups[state->num_groups-1] + num;
        }
        break;

        case OPCODE_GOVERWRITE:
        {
            int current = state->groups[state->num_groups-1];
            int parent  = state->groups[state->num_groups-2];

            int current_size = state->eval_depth - current;

            for (int i = 0; i < current_size; i++)
                state->eval_stack[parent + i] = state->eval_stack[current + i];

            state->num_groups--;
            state->eval_depth = parent + current_size;
        }
        break;

        case OPCODE_GPACK:
        {
            Value array = make_array(state->a);
            if (array == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            for (int i = state->groups[state->num_groups-1]; i < state->eval_depth; i++) {
                int ret = array_append(state->a, array, state->eval_stack[i]);
                if (ret < 0) {
                    eval_report(state, "Out of memory");
                    return -1;
                }
            }

            state->eval_depth = state->groups[--state->num_groups];
            state->eval_stack[state->eval_depth++] = array;
        }
        break;

        case OPCODE_PUSHN:
        {
            state->eval_stack[state->eval_depth++] = VALUE_NONE;
        }
        break;

        case OPCODE_PUSHI:
        {
            int64_t x = read_s64(state);

            Value v = make_int(state->a, x);
            if (v == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = v;
        }
        break;

        case OPCODE_PUSHF:
        {
            double x = read_f64(state);

            Value v = make_float(state->a, x);
            if (v == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = v;
        }
        break;

        case OPCODE_PUSHS:
        {
            uint32_t off = read_u32(state);
            uint32_t len = read_u32(state);

            Value v = make_str(state->a, (String) { state->data.ptr + off, len });
            if (v == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = v;
        }
        break;

        case OPCODE_PUSHT:
        {
            state->eval_stack[state->eval_depth++] = VALUE_TRUE;
        }
        break;

        case OPCODE_PUSHFL:
        {
            state->eval_stack[state->eval_depth++] = VALUE_FALSE;
        }
        break;

        case OPCODE_PUSHV:
        {
            uint8_t idx = read_u8(state);

            int group = state->frames[state->num_frames-1].group;
            Value v = state->eval_stack[state->groups[group] + idx];

            state->eval_stack[state->eval_depth++] = v;
        }
        break;

        case OPCODE_PUSHA:
        {
            uint32_t cap = read_u32(state);
            (void) cap;

            Value v = make_array(state->a);
            if (v == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = v;
        }
        break;

        case OPCODE_PUSHM:
        {
            uint32_t cap = read_u32(state);
            (void) cap;

            Value v = make_map(state->a);
            if (v == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = v;
        }
        break;

        case OPCODE_POP:
        {
            assert(state->num_groups == 0 || state->eval_depth > state->groups[state->num_groups-1]);
            state->eval_depth--;
        }
        break;

        case OPCODE_NEG:
        {
            Value a = state->eval_stack[--state->eval_depth];
            Type  t = type_of(a);

            Value r;
            if (0) {}
            else if (t == TYPE_INT)   r = make_int(state->a, -get_int(a));
            else if (t == TYPE_FLOAT) r = make_float(state->a, -get_float(a));
            else {
                eval_report(state, "Invalid operation on non-numeric value");
                return -1;
            }

            if (r == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = r;
        }
        break;

        case OPCODE_EQL:
        {
            Value a = state->eval_stack[state->eval_depth-2];
            Value b = state->eval_stack[state->eval_depth-1];
            state->eval_depth -= 2;

            Value r = valeq(a, b) ? VALUE_TRUE : VALUE_FALSE;
            state->eval_stack[state->eval_depth++] = r;
        }
        break;

        case OPCODE_NQL:
        {
            Value a = state->eval_stack[state->eval_depth-2];
            Value b = state->eval_stack[state->eval_depth-1];
            state->eval_depth -= 2;

            Value r = valeq(a, b) ? VALUE_FALSE : VALUE_TRUE;
            state->eval_stack[state->eval_depth++] = r;
        }
        break;

        case OPCODE_LSS:
        {
            Value a = state->eval_stack[state->eval_depth-2];
            Value b = state->eval_stack[state->eval_depth-1];
            state->eval_depth -= 2;

            if (type_of(a) != TYPE_INT || type_of(b) != TYPE_INT) {
                eval_report(state, "Invalid operation on non-numeric value");
                return -1;
            }

            Value r = valgrt(a, b) || valeq(a, b) ? VALUE_FALSE : VALUE_TRUE;
            state->eval_stack[state->eval_depth++] = r;
        }
        break;

        case OPCODE_GRT:
        {
            Value a = state->eval_stack[state->eval_depth-2];
            Value b = state->eval_stack[state->eval_depth-1];
            state->eval_depth -= 2;

            if (type_of(a) != TYPE_INT || type_of(b) != TYPE_INT) {
                eval_report(state, "Invalid operation on non-numeric value");
                return -1;
            }

            Value r = valgrt(a, b) ? VALUE_TRUE : VALUE_FALSE;
            state->eval_stack[state->eval_depth++] = r;
        }
        break;

        case OPCODE_ADD:
        {
            Value a = state->eval_stack[state->eval_depth-2];
            Value b = state->eval_stack[state->eval_depth-1];
            state->eval_depth -= 2;

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
                    r = make_int(state->a, u + v);
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(state->a, u + v);
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(state->a, u + v);
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    // TODO: check overflow and underflow
                    r = make_float(state->a, u + v);
                }
                break;

                default:
                eval_report(state, "Invalid operation on non-numeric value");
                return -1;
            }

            if (r == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = r;
        }
        break;

        case OPCODE_SUB:
        {
            Value a = state->eval_stack[state->eval_depth-2];
            Value b = state->eval_stack[state->eval_depth-1];
            state->eval_depth -= 2;

            Type t1 = type_of(a);
            Type t2 = type_of(b);

            Value r;
            switch (TYPE_PAIR(t1, t2)) {

                case TYPE_PAIR(TYPE_INT, TYPE_INT):
                {
                    int64_t u = get_int(a);
                    int64_t v = get_int(b);
                    // TODO: check overflow and underflow
                    r = make_int(state->a, u - v);
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(state->a, u - v);
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(state->a, u - v);
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    // TODO: check overflow and underflow
                    r = make_float(state->a, u - v);
                }
                break;

                default:
                eval_report(state, "Invalid operation on non-numeric value");
                return -1;
            }

            if (r == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = r;
        }
        break;

        case OPCODE_MUL:
        {
            Value a = state->eval_stack[state->eval_depth-2];
            Value b = state->eval_stack[state->eval_depth-1];
            state->eval_depth -= 2;

            Type t1 = type_of(a);
            Type t2 = type_of(b);

            Value r;
            switch (TYPE_PAIR(t1, t2)) {

                case TYPE_PAIR(TYPE_INT, TYPE_INT):
                {
                    int64_t u = get_int(a);
                    int64_t v = get_int(b);
                    // TODO: check overflow and underflow
                    r = make_int(state->a, u * v);
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(state->a, u * v);
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(state->a, u * v);
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    // TODO: check overflow and underflow
                    r = make_float(state->a, u * v);
                }
                break;

                default:
                eval_report(state, "Invalid operation on non-numeric value");
                return -1;
            }

            if (r == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = r;
        }
        break;

        case OPCODE_DIV:
        {
            Value a = state->eval_stack[state->eval_depth-2];
            Value b = state->eval_stack[state->eval_depth-1];
            state->eval_depth -= 2;

            Type t1 = type_of(a);
            Type t2 = type_of(b);

            Value r;
            switch (TYPE_PAIR(t1, t2)) {

                case TYPE_PAIR(TYPE_INT, TYPE_INT):
                {
                    // TODO: check division by 0

                    int64_t u = get_int(a);
                    int64_t v = get_int(b);
                    r = make_int(state->a, u / v);
                }
                break;

                case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
                {
                    // TODO: check division by 0

                    float u = (float) get_int(a);
                    float v = get_float(b);
                    r = make_float(state->a, u / v);
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
                {
                    // TODO: check division by 0

                    float u = get_float(a);
                    float v = (float) get_int(b);
                    r = make_float(state->a, u / v);
                }
                break;

                case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
                {
                    float u = get_float(a);
                    float v = get_float(b);
                    r = make_float(state->a, u / v);
                }
                break;

                default:
                eval_report(state, "Invalid operation on non-numeric value");
                return -1;
            }

            if (r == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = r;
        }
        break;

        case OPCODE_MOD:
        {
            Value a = state->eval_stack[state->eval_depth-2];
            Value b = state->eval_stack[state->eval_depth-1];
            state->eval_depth -= 2;

            Type t1 = type_of(a);
            Type t2 = type_of(b);

            if (t1 != TYPE_INT || t2 != TYPE_INT) {
                eval_report(state, "Invalid modulo operation on non-integer value");
                return -1;
            }

            int64_t u = get_int(a);
            int64_t v = get_int(b);
            Value r = make_int(state->a, u % v);
            if (r == VALUE_ERROR) {
                eval_report(state, "Out of memory");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = r;
        }
        break;

        case OPCODE_SETV:
        {
            uint8_t x = read_u8(state);

            Frame *f = &state->frames[state->num_frames-1];
            state->eval_stack[state->groups[f->group] + x] = state->eval_stack[--state->eval_depth];
        }
        break;

        case OPCODE_JUMP:
        {
            uint32_t x = read_u32(state);
            state->off = x;
        }
        break;

        case OPCODE_JIFP:
        {
            uint32_t x = read_u32(state);
            Value a = state->eval_stack[--state->eval_depth];

            if (a == VALUE_FALSE)
                state->off = x;
            else {
                if (a != VALUE_TRUE) {
                    eval_report(state, "Invalid operation on non-boolean value");
                    return -1;
                }
            }
        }
        break;

        case OPCODE_CALL:
        {
            uint32_t off = read_u32(state);

            if (state->num_frames == FRAME_LIMIT) {
                eval_report(state, "Frame limit reached");
                return -1;
            }
            state->frames[state->num_frames++] = (Frame) {.return_addr=state->off, .group=state->num_groups-1};

            state->off = off;
        }
        break;

        case OPCODE_RET:
        {
            state->off = state->frames[--state->num_frames].return_addr;
        }
        break;

        case OPCODE_APPEND:
        {
            Value val = state->eval_stack[state->eval_depth-1];
            Value set = state->eval_stack[state->eval_depth-2];
            state->eval_depth--;

            if (type_of(set) != TYPE_ARRAY) {
                eval_report(state, "Invalid operation on non-array value");
                return -1;
            }

            int ret = array_append(state->a, set, val);
            if (ret < 0) {
                eval_report(state, "Out of memory");
                return -1;
            }
        }
        break;

        case OPCODE_INSERT1:
        {
            Value key = state->eval_stack[state->eval_depth-1];
            Value val = state->eval_stack[state->eval_depth-2];
            Value set = state->eval_stack[state->eval_depth-3];
            state->eval_depth -= 2;

            if (type_of(set) == TYPE_ARRAY) {

                if (type_of(key) != TYPE_INT) {
                    assert(0); // TODO
                }
                int64_t idx = get_int(key);

                Value *dst = array_select(set, idx);
                if (dst == NULL) {
                    eval_report(state, "Index out of range");
                    return -1;
                }
                *dst = val;

            } else if (type_of(set) == TYPE_MAP) {

                int ret = map_insert(state->a, set, key, val);
                if (ret < 0) {
                    eval_report(state, "Out of memory");
                    return -1;
                }

            } else {
                eval_report(state, "Invalid insertion on non-array and non-map value");
                return -1;
            }
        }
        break;

        case OPCODE_INSERT2:
        {
            Value key = state->eval_stack[state->eval_depth-1];
            Value set = state->eval_stack[state->eval_depth-2];
            Value val = state->eval_stack[state->eval_depth-3];
            state->eval_depth -= 2;

            if (type_of(set) == TYPE_ARRAY) {

                if (type_of(key) != TYPE_INT) {
                    assert(0); // TODO
                }
                int64_t idx = get_int(key);

                Value *dst = array_select(set, idx);
                if (dst == NULL) {
                    eval_report(state, "Index out of range");
                    return -1;
                }
                *dst = val;

            } else if (type_of(set) == TYPE_MAP) {

                int ret = map_insert(state->a, set, key, val);
                if (ret < 0) {
                    eval_report(state, "Out of memory");
                    return -1;
                }

            } else {
                eval_report(state, "Invalid insertion on non-array and non-map value");
                return -1;
            }
        }
        break;

        case OPCODE_SELECT:
        {
            Value key = state->eval_stack[state->eval_depth-1];
            Value set = state->eval_stack[state->eval_depth-2];
            state->eval_depth -= 2;

            Value r;
            if (type_of(set) == TYPE_ARRAY) {

                if (type_of(key) != TYPE_INT) {
                    assert(0); // TODO
                }
                int64_t idx = get_int(key);

                Value *src = array_select(set, idx);
                if (src == NULL) {
                    eval_report(state, "Index out of range");
                    return -1;
                }
                r = *src;

            } else if (type_of(set) == TYPE_MAP) {

                int ret = map_select(set, key, &r);
                if (ret < 0) {
                    eval_report(state, "Key not contained in map");
                    return -1;
                }

            } else {
                eval_report(state, "Invalid selection from non-array and non-map value");
                return -1;
            }

            state->eval_stack[state->eval_depth++] = r;
        }
        break;

        case OPCODE_PRINT:
        {
            state->num_prints = 1;
        }
        break;

        case OPCODE_SYSVAR:
        {
            uint32_t off = read_u32(state);
            uint32_t len = read_u32(state);
            String name = { state->data.ptr + off, len };

            state->sysvar = name;
            state->stack_before_user = state->eval_depth;
            state->stack_base_for_user = state->groups[state->num_groups-1];
        }
        break;

        case OPCODE_SYSCALL:
        {
            uint32_t off = read_u32(state);
            uint32_t len = read_u32(state);
            String name = { state->data.ptr + off, len };

            int num_args = state->eval_depth - state->groups[state->num_groups-1];

            Value v = make_int(state->a, num_args);
            if (v == VALUE_ERROR) {
                assert(0); // TODO
            }
            state->eval_stack[state->eval_depth++] = v;

            state->syscall = name;
            state->stack_before_user = state->eval_depth;
            state->stack_base_for_user = state->groups[state->num_groups-1];
        }
        break;

        case OPCODE_FOR:
        {
            uint8_t  var_3 = read_u8(state);
            uint8_t  var_1 = read_u8(state);
            uint8_t  var_2 = read_u8(state);
            uint32_t end   = read_u32(state);

            int base;
            {
                int group = state->frames[state->num_frames-1].group;
                base  = state->groups[group];
            }

            int64_t idx;
            {
                Value idx_val = state->eval_stack[base + var_2];
                if (type_of(idx_val) != TYPE_INT) {
                    assert(0); // TODO
                }
                idx = get_int(idx_val);
            }

            Value set = state->eval_stack[base + var_3];

            Type set_type = type_of(set);
            if (set_type == TYPE_ARRAY) {

                if (value_length(set) == idx) {
                    state->off = end;
                    break;
                }

                state->eval_stack[base + var_1] = *array_select(set, idx);

            } else if (set_type == TYPE_MAP) {

                if (value_length(set) == idx) {
                    state->off = end;
                    break;
                }

                state->eval_stack[base + var_1] = *map_select_by_index(set, idx);

            } else {
                assert(0); // TODO
            }

            Value v = make_int(state->a, idx + 1);
            if (v == VALUE_ERROR) {
                assert(0); // TODO
            }
            state->eval_stack[base + var_2] = v;
        }
        break;

        default:
        eval_report(state, "Invalid opcode (offset %d)", state->off-1);
        return -1;
    }

    return 0;
}

WL_State *WL_State_init(WL_Arena *a, WL_Program p, char *err, int errmax)
{
    WL_State *state = alloc(a, (int) sizeof(WL_State), _Alignof(WL_State));
    if (state == NULL)
        return NULL;

    String code;
    String data;

    int ret = parse_program_header(p, &code, &data, err, errmax);
    if (ret < 0)
        return NULL;

    *state = (WL_State) {
        .code=code,
        .data=data,
        .off=0,
        .trace=false,
        .a=a,
        .errbuf=err,
        .errmax=errmax,
        .errlen=0,
        .num_frames=0,
        .eval_depth=0,
        .num_groups=0,
        .num_prints=0,
        .cur_print=0,
    };

    state->frames[state->num_frames++] = (Frame) { 0, 0 };

    return state;
}

void WL_State_free(WL_State *state)
{
    state->num_frames--;

    // TODO
}

void WL_State_trace(WL_State *state, int trace)
{
    state->trace = (trace != 0);
}

WL_Result WL_eval(WL_State *state)
{
    if (state->sysvar.len > 0) {

        if (state->syscall_error)
            return (WL_Result) { WL_ERROR, (WL_String) { NULL, 0 } };

        state->sysvar = S("");
    }

    if (state->syscall.len > 0) {

        if (state->syscall_error)
            return (WL_Result) { WL_ERROR, (WL_String) { NULL, 0 } };

        int group = state->groups[state->num_groups-1];

        Value v = state->eval_stack[--state->eval_depth];
        if (type_of(v) != TYPE_INT) {
            assert(0); // TODO
        }
        int64_t num_rets = get_int(v);
        for (int i = 0; i < num_rets; i++)
            state->eval_stack[group + i] = state->eval_stack[state->eval_depth - num_rets + i];

        state->eval_depth = group + num_rets;

        state->syscall = S("");
    }

    while (state->num_prints == 0) {

        int ret = step(state);
        if (ret < 0)  return (WL_Result) { WL_ERROR, (WL_String) { NULL, 0 } };
        if (ret == 1) return (WL_Result) { WL_DONE,  (WL_String) { NULL, 0 } };

        if (state->sysvar.len > 0)
            return (WL_Result) { WL_VAR, (WL_String) { state->sysvar.ptr, state->sysvar.len } };

        if (state->syscall.len > 0)
            return (WL_Result) { WL_CALL, (WL_String) { state->syscall.ptr, state->syscall.len } };
    }

    Value v = state->eval_stack[state->eval_depth - state->num_prints + state->cur_print];
        
    state->cur_print++;
    if (state->cur_print == state->num_prints) {
        state->cur_print = 0;
        state->num_prints = 0;
    }

    WL_String str;

    if (type_of(v) == TYPE_STRING) {
        String str2 = get_str(v);
        str.ptr = str2.ptr;
        str.len = str2.len;
    } else {
        int   cap = 8;
        char *dst = alloc(state->a, cap, 1);
        int   len = value_to_string(v, dst, cap);
        if (len > cap) {
            if (!grow_alloc(state->a, dst, len)) {
                assert(0); // TODO
            }
            value_to_string(v, dst, len);
        }
        str.ptr = dst;
        str.len = len;
    }

    return (WL_Result) { WL_OUTPUT, str };
}

static bool in_syscall(WL_State *state)
{
    return (state->syscall.len > 0 || state->sysvar.len > 0) && !state->syscall_error;
}

int WL_peeknone(WL_State *state, int off)
{
    if (!in_syscall(state)) return 0;

    if (state->eval_depth + off < state->stack_base_for_user || off >= 0)
        return 0;

    Value v = state->eval_stack[state->eval_depth + off];
    if (type_of(v) != TYPE_NONE)
        return 0;

    return 1;
}

int WL_peekint(WL_State *state, int off, long long *x)
{
    if (!in_syscall(state)) return 0;

    if (state->eval_depth + off < state->stack_base_for_user || off >= 0)
        return 0;

    Value v = state->eval_stack[state->eval_depth + off];
    if (type_of(v) != TYPE_INT)
        return 0;

    *x = get_int(v);
    return 1;
}

int WL_peekfloat(WL_State *state, int off, float *x)
{
    if (!in_syscall(state)) return 0;

    if (state->eval_depth + off < state->stack_base_for_user || off >= 0)
        return 0;

    Value v = state->eval_stack[state->eval_depth + off];
    if (type_of(v) != TYPE_FLOAT)
        return 0;

    *x = get_float(v);
    return 1;
}

int WL_peekstr(WL_State *state, int off, WL_String *str)
{
    if (!in_syscall(state)) return 0;

    if (state->eval_depth + off < state->stack_base_for_user || off >= 0)
        return 0;

    Value v = state->eval_stack[state->eval_depth + off];
    if (type_of(v) != TYPE_STRING)
        return 0;

    String s = get_str(v);
    *str = (WL_String) { s.ptr, s.len };
    return 1;
}

int WL_popnone(WL_State *state)
{
    if (!in_syscall(state)) return 0;

    if (state->eval_depth == state->stack_base_for_user)
        return 0;

    Value v = state->eval_stack[state->eval_depth-1];
    if (type_of(v) != TYPE_NONE)
        return 0;

    state->eval_depth--;
    return 1;
}

int WL_popint(WL_State *state, long long *x)
{
    if (!in_syscall(state)) return 0;

    if (state->eval_depth == state->stack_base_for_user)
        return 0;

    Value v = state->eval_stack[state->eval_depth-1];
    if (type_of(v) != TYPE_INT)
        return 0;

    *x = get_int(v);

    state->eval_depth--;
    return 1;
}

int WL_popfloat(WL_State *state, float *x)
{
    if (!in_syscall(state)) return 0;

    if (state->eval_depth == state->stack_base_for_user)
        return 0;

    Value v = state->eval_stack[state->eval_depth-1];
    if (type_of(v) != TYPE_FLOAT)
        return 0;

    *x = get_float(v);

    state->eval_depth--;
    return 1;
}

int WL_popstr(WL_State *state, WL_String *str)
{
    if (!in_syscall(state)) return 0;

    if (state->eval_depth == state->stack_base_for_user)
        return 0;

    Value v = state->eval_stack[state->eval_depth-1];
    if (type_of(v) != TYPE_STRING)
        return 0;

    String s = get_str(v);
    *str = (WL_String) { s.ptr, s.len };

    state->eval_depth--;
    return 1;
}

int WL_popany(WL_State *state)
{
    if (!in_syscall(state))
        return 0;

    if (state->eval_depth == state->stack_base_for_user)
        return 0;

    state->eval_depth--;
    return 1;
}

void WL_select(WL_State *state)
{
    Value key = state->eval_stack[--state->eval_depth];
    Value set = state->eval_stack[state->eval_depth-1];
    Value val;

    Type set_type = type_of(set);
    if (set_type == TYPE_ARRAY) {

        Type key_type = type_of(key);
        if (key_type != TYPE_INT) {
            assert(0); // TODO
        }

        int64_t idx = get_int(key);
        Value *src = array_select(set, idx);
        if (src == NULL) {
            assert(0); // TODO
        }
        val = *src;

    } else if (set_type == TYPE_MAP) {

        int ret = map_select(set, key, &val);
        if (ret < 0) {
            assert(0); // TODO
        }

    } else {

        assert(0); // TODO
    }

    state->eval_stack[state->eval_depth++] = val;
}

void WL_pushnone(WL_State *state)
{
    if (!in_syscall(state)) return;

    state->eval_stack[state->eval_depth++] = VALUE_NONE;
}

void WL_pushint(WL_State *state, long long x)
{
    if (!in_syscall(state)) return;

    Value v = make_int(state->a, x);
    if (v == VALUE_ERROR) {
        eval_report(state, "Out of memory");
        state->syscall_error = true;
        return;
    }

    state->eval_stack[state->eval_depth++] = v;
}

void WL_pushfloat(WL_State *state, float x)
{
    if (!in_syscall(state)) return;

    Value v = make_float(state->a, x);
    if (v == VALUE_ERROR) {
        eval_report(state, "Out of memory");
        state->syscall_error = true;
        return;
    }

    state->eval_stack[state->eval_depth++] = v;
}

void WL_pushstr(WL_State *state, WL_String str)
{
    if (!in_syscall(state)) return;

    Value v = make_str(state->a, (String) { str.ptr, str.len });
    if (v == VALUE_ERROR) {
        eval_report(state, "Out of memory");
        state->syscall_error = true;
        return;
    }

    state->eval_stack[state->eval_depth++] = v;
}

void WL_pusharray(WL_State *state, int cap)
{
    if (!in_syscall(state)) return;

    (void) cap;
    Value v = make_array(state->a);
    if (v == VALUE_ERROR) {
        eval_report(state, "Out of memory");
        state->syscall_error = true;
        return;
    }

    state->eval_stack[state->eval_depth++] = v;
}

void WL_pushmap(WL_State *state, int cap)
{
    if (!in_syscall(state)) return;

    (void) cap;
    Value v = make_map(state->a);
    if (v == VALUE_ERROR) {
        eval_report(state, "Out of memory");
        state->syscall_error = true;
        return;
    }

    state->eval_stack[state->eval_depth++] = v;
}

void WL_insert(WL_State *state)
{
    Value key = state->eval_stack[--state->eval_depth];
    Value val = state->eval_stack[--state->eval_depth];
    Value set = state->eval_stack[state->eval_depth-1];

    Type set_type = type_of(set);
    if (set_type == TYPE_ARRAY) {

        Type key_type = type_of(key);
        if (key_type != TYPE_INT) {
            assert(0); // TODO
        }

        int64_t idx = get_int(key);
        Value *dst = array_select(set, idx);
        if (dst == NULL) {
            assert(0); // TODO
        }
        *dst = val;

    } else if (set_type == TYPE_MAP) {

        int ret = map_insert(state->a, set, key, val);
        if (ret < 0) {
            assert(0); // TODO
        }

    } else {

        assert(0); // TODO
    }
}

void WL_append(WL_State *state)
{
    Value val = state->eval_stack[--state->eval_depth];
    Value set = state->eval_stack[state->eval_depth-1];

    if (type_of(set) != TYPE_ARRAY) {
        assert(0); // TODO
        return;
    }

    if (array_append(state->a, set, val) < 0) {
        assert(0); // TODO
    }
}
