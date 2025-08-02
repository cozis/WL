#include "eval.h"

#define FRAME_LIMIT 128
#define EVAL_STACK_LIMIT 128

typedef struct {
} Frame;

typedef union {
    uint64_t i;
    void*    p;
} EvalStackItem;

typedef struct {
    Program p;
    int off;

    int frame_depth;
    Frame frames[FRAME_LIMIT];

    int eval_depth;
    EvalStackItem eval_stack[EVAL_STACK_LIMIT];

} Eval;

void step(Eval *e)
{
    uint8_t b = e->p.ptr[e->off++];

    switch (b) {

        case OPCODE_NOPE:
        // Do nothing
        break;

        case OPCODE_PUSHI:
        {
            int64_t x;

            memcpy(&x, e->p.ptr + e->off, sizeof(x));
            e->off += (int) sizeof(x);

            e->eval_stack[e->eval_depth++] = x;
        }
        break;

        case OPCODE_PUSHF:
        {
            double x;

            memcpy(&x, e->p.ptr + e->off, sizeof(x));
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

            // TODO

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_EQL:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            // TODO

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_NQL:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            // TODO

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_LSS:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            // TODO

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_GRT:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            // TODO

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_ADD:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            // TODO

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_SUB:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            // TODO

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_MUL:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            // TODO

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_DIV:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            // TODO

            e->eval_stack[e->eval_depth++] = r;
        }
        break;

        case OPCODE_MOD:
        {
            Value a = e->eval_stack[e->eval_depth-2];
            Value b = e->eval_stack[e->eval_depth-1];
            e->eval_depth -= 2;

            // TODO

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

            memcpy(&x, e->p.ptr + e->off, (int) sizeof(x));
            e->off = (int) sizeof(x);
        }
        break;

        case OPCODE_JIFP:
        {
            // TODO
        }
        break;

        case OPCODE_SCALL:
        {
            // TODO
        }
        break;

        case OPCODE_DCALL:
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