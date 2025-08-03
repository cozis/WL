#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef WL_AMALGAMATION
#include "parse.h"
#include "assemble.h"
#endif

#define MAX_SCOPES 32
#define MAX_VARS   1024
#define MAX_STRING_LITERALS 1024

typedef struct FunctionCall FunctionCall;
struct FunctionCall {
    FunctionCall *next;
    String        name;
    int           off;
};

typedef struct {
    String name;
} Variable;

typedef enum {
    SCOPE_FUNC,
    SCOPE_FOR,
    SCOPE_WHILE,
    SCOPE_IF,
    SCOPE_ELSE,
} ScopeType;

typedef struct {
    ScopeType type;
    int var_base;
    // TODO
    FunctionCall *calls;
} Scope;

typedef struct {
    char *ptr;
    int   len;
    int   cap;
    b32   err;
} OutputBuffer;

typedef struct {

    Arena *a;

    OutputBuffer out;

    int num_vars;
    Variable vars[MAX_VARS];

    int num_scopes;
    Scope scopes[MAX_SCOPES];

    int strings_len;
    int strings_cap;
    char *strings;

    char *errbuf;
    int   errmax;
    int   errlen;

} Assembler;

void assembler_report(Assembler *a, char *fmt, ...)
{
    if (a->errmax == 0 || a->errlen > 0)
        return;

    int len = snprintf(a->errbuf, a->errmax, "Error: ");
    if (len < 0) {
        // TODO
    }

    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(a->errbuf + len, a->errmax - len, fmt, args);
    va_end(args);
    if (ret < 0) {
        // TODO
    }
    len += ret;

    a->errlen = len;
}

int add_string_literal(Assembler *a, String str)
{
    if (a->strings_cap - a->strings_len < str.len) {
        int c = MAX(2 * a->strings_cap, a->strings_len + str.len);
        char *p = malloc(c);
        if (p == NULL) {
            assembler_report(a, "Out of memory");
            return -1;
        }
        if (a->strings_cap) {
            memcpy(p, a->strings, a->strings_len);
            free(a->strings);
        }

        a->strings = p;
        a->strings_cap = c;
    }

    int off = a->strings_len;
    memcpy(a->strings + a->strings_len, str.ptr, str.len);
    a->strings_len += str.len;

    return off;
}

void append_mem(OutputBuffer *out, void *ptr, int len)
{
    if (out->err)
        return;

    if (out->cap - out->len < len) {

        int   new_cap = MAX(512, 2 * out->cap);
        char *new_ptr = malloc(new_cap);
        if (new_ptr == NULL) {
            out->err = true;
            return;
        }

        if (out->cap) {
            memcpy(new_ptr, out->ptr, out->len);
            free(out->ptr);
        }

        out->ptr = new_ptr;
        out->cap = new_cap;
    }

    memcpy(out->ptr + out->len, ptr, len);
    out->len += len;
}

void patch_mem(OutputBuffer *out, int off, void *ptr, int len)
{
    if (out->err)
        return;

    memcpy(out->ptr + off, ptr, len);
}

int append_u8(OutputBuffer *out, uint8_t x)
{
    int off = out->len;
    append_mem(out, &x, (int) sizeof(x));
    return off;
}

int append_u32(OutputBuffer *out, uint32_t x)
{
    int off = out->len;
    append_mem(out, &x, (int) sizeof(x));
    return off;
}

int append_u64(OutputBuffer *out, uint64_t x)
{
    int off = out->len;
    append_mem(out, &x, (int) sizeof(x));
    return off;
}

int append_f64(OutputBuffer *out, double x)
{
    int off = out->len;
    append_mem(out, &x, (int) sizeof(x));
    return off;
}

void patch_with_current_offset(OutputBuffer *out, int off)
{
    uint32_t x = out->len;
    patch_mem(out, off, &x, (int) sizeof(x));
}

int current_offset(OutputBuffer *out)
{
    return out->len;
}

int count_nodes(Node *head)
{
    int n = 0;
    Node *node = head;
    while (node) {
        n++;
        node = node->next;
    }
    return n;
}

int find_variable(Assembler *a, String name)
{
    Scope *scope = &a->scopes[a->num_scopes-1];
    for (int i = scope->var_base; i < a->num_vars; i++)
        if (streq(name, a->vars[i].name))
            return i;
    assembler_report(a, "Undeclared variable '%.*s'", name.len, name.ptr);
    return -1;
}

int declare_var(Assembler *a, String name)
{
    if (a->num_vars == MAX_VARS) {
        assembler_report(a, "Variable limit reached");
        return -1;
    }

    Scope *scope = &a->scopes[a->num_scopes-1];
    for (int i = scope->var_base; i < a->num_vars; i++)
        if (streq(name, a->vars[i].name)) {
            assembler_report(a, "Variable '%.*s' declared twice", name.len, name.ptr);
            return -1;
        }

    int idx = a->num_vars++;
    a->vars[idx] = (Variable) { name };
    return idx - scope->var_base;
}

/*
void assemble_html_node_inner(Assembler *a, Node *node, OutputBuffer *tmp)
{
    append_u8(tmp, '<');
    append_mem(tmp, node->tagname.ptr, node->tagname.len);

    Node *attr = node->params;
    while (attr) {
        append_u8(tmp, ' ');
        append_mem(tmp, attr->attr_name.ptr, attr->attr_name.len);
        if (attr->attr_value) {
            append_u8(tmp, '=');
            // TODO
        }
        attr = attr->next;
    }
    append_u8(tmp, '>');

    Node *child = node->child;
    while (child) {
        // TODO
        child = child->next;
    }

    append_u8(tmp, '<');
    append_u8(tmp, '/');
    append_mem(tmp, node->tagname.ptr, node->tagname.len);
    append_u8(tmp, '>');
}

void assemble_html_node(Assembler *a, Node *node)
{
    OutputBuffer tmp;
    assemble_html_node_inner(a, node, &tmp);
}
*/

b32 is_expr(Node *node)
{
    switch (node->type) {

        default:
        break;

        case NODE_SELECT:
        case NODE_FUNC_CALL:
        case NODE_OPER_POS:
        case NODE_OPER_NEG:
        case NODE_OPER_ASS:
        case NODE_OPER_EQL:
        case NODE_OPER_NQL:
        case NODE_OPER_LSS:
        case NODE_OPER_GRT:
        case NODE_OPER_ADD:
        case NODE_OPER_SUB:
        case NODE_OPER_MUL:
        case NODE_OPER_DIV:
        case NODE_OPER_MOD:
        case NODE_VALUE_INT:
        case NODE_VALUE_FLOAT:
        case NODE_VALUE_STR:
        case NODE_VALUE_VAR:
        case NODE_VALUE_HTML:
        case NODE_VALUE_ARRAY:
        case NODE_VALUE_MAP:
        return true;
    }

    return false;
}

void assemble_node(Assembler *a, Node *node)
{
    switch (node->type) {

        case NODE_FUNC_DECL:
        {
            append_u8(&a->out, OPCODE_JUMP);
            int p1 = append_u32(&a->out, 0);

            // TODO: resolve references

            int arg_count = count_nodes(node->func_args);

            Node *args[32];
            if (arg_count > COUNT(args)) {
                assembler_report(a, "Argument limit reached");
                return;
            }

            for (int i = arg_count-1; i >= 0; i--) {

                int idx = declare_var(a, args[i]->sval);
                if (idx < 0) return;

                append_u8(&a->out, OPCODE_SETV);
                append_u8(&a->out, idx);
            }

            if (is_expr(node->func_body)) {
                assemble_node(a, node->func_body);
                append_u8(&a->out, OPCODE_RET);
            } else
                assemble_node(a, node->func_body);

            patch_with_current_offset(&a->out, p1);
        }
        break;

        case NODE_FUNC_CALL:
        {
            Node *func = node->left;
            Node *args = node->right;

            int arg_count = 0;
            Node *arg = args;
            while (arg) {
                assemble_node(a, arg);
                arg_count++;
                arg = arg->next;
            }

            if (func->type == NODE_VALUE_VAR) {
                assemble_node(a, arg);
                append_u8(&a->out, OPCODE_SCALL);
                append_u8(&a->out, arg_count);
                int p = append_u32(&a->out, 0);

                FunctionCall *call = alloc(a->a, sizeof(FunctionCall), _Alignof(FunctionCall));
                if (call == NULL) {
                    assembler_report(a, "Out of memory");
                    return;
                }
                call->name = func->sval;
                call->off = p;

                Scope *scope = &a->scopes[a->num_scopes-1];

                call->next = scope->calls;
                scope->calls = call;

            } else {
                assemble_node(a, func);
                assemble_node(a, arg);
                append_u8(&a->out, OPCODE_DCALL);
                append_u8(&a->out, arg_count);
            }
        }
        break;

        case NODE_VAR_DECL:
        {
            int idx = declare_var(a, node->var_name);
            if (idx < 0) return;

            if (node->var_value) {
                assemble_node(a, node->var_value);
                append_u8(&a->out, OPCODE_SETV);
                append_u8(&a->out, idx);
            }
        }
        break;

        case NODE_BLOCK:
        {
            Node *stmt = node->child;
            while (stmt) {
                assemble_node(a, stmt);
                if (is_expr(stmt))
                    append_u8(&a->out, OPCODE_POP);
                stmt = stmt->next;
            }
        }
        break;

        case NODE_IFELSE:
        {
            // If there is no else branch:
            //
            //   <cond>
            //   JIFP end
            //   <left>
            // end:
            //   ...
            //
            // If there is:
            //
            //   <cond>
            //   JIFP else
            //   <left>
            //   JUMP end
            // else:
            //   <right>
            // end:
            //   ...

            if (node->right) {

                assemble_node(a, node->cond);

                append_u8(&a->out, OPCODE_JIFP);
                int p1 = append_u32(&a->out, 0);

                assemble_node(a, node->left);

                append_u8(&a->out, OPCODE_JUMP);
                int p2 = append_u32(&a->out, 0);

                patch_with_current_offset(&a->out, p1);

                assemble_node(a, node->right);

                patch_with_current_offset(&a->out, p2);

            } else {

                assemble_node(a, node->cond);

                append_u8(&a->out, OPCODE_JIFP);
                int p1 = append_u32(&a->out, 0);

                assemble_node(a, node->left);

                patch_with_current_offset(&a->out, p1);
            }
        }
        break;

        case NODE_WHILE:
        {
            // start:
            //   <cond>
            //   JIFP end
            //   <body>
            //   JUMP start
            // end:
            //   ...

            int start = current_offset(&a->out);

            assemble_node(a, node->cond);

            append_u8(&a->out, OPCODE_JIFP);
            int p = append_u32(&a->out, 0);

            assemble_node(a, node->left);

            append_u8(&a->out, OPCODE_JUMP);
            append_u32(&a->out, start);

            patch_with_current_offset(&a->out, p);
        }
        break;

        case NODE_FOR:
        {
            assemble_node(a, node->for_set);
            // TODO
        }
        break;

        case NODE_OPER_POS:
        assemble_node(a, node->left);
        break;

        case NODE_OPER_NEG:
        assemble_node(a, node->left);
        append_u8(&a->out, OPCODE_NEG);
        break;

        case NODE_OPER_EQL:
        assemble_node(a, node->left);
        assemble_node(a, node->right);
        append_u8(&a->out, OPCODE_EQL);
        break;

        case NODE_OPER_NQL:
        assemble_node(a, node->left);
        assemble_node(a, node->right);
        append_u8(&a->out, OPCODE_NQL);
        break;

        case NODE_OPER_LSS:
        assemble_node(a, node->left);
        assemble_node(a, node->right);
        append_u8(&a->out, OPCODE_LSS);
        break;

        case NODE_OPER_GRT:
        assemble_node(a, node->left);
        assemble_node(a, node->right);
        append_u8(&a->out, OPCODE_GRT);
        break;

        case NODE_OPER_ADD:
        assemble_node(a, node->left);
        assemble_node(a, node->right);
        append_u8(&a->out, OPCODE_ADD);
        break;

        case NODE_OPER_SUB:
        assemble_node(a, node->left);
        assemble_node(a, node->right);
        append_u8(&a->out, OPCODE_SUB);
        break;

        case NODE_OPER_MUL:
        assemble_node(a, node->left);
        assemble_node(a, node->right);
        append_u8(&a->out, OPCODE_MUL);
        break;

        case NODE_OPER_DIV:
        assemble_node(a, node->left);
        assemble_node(a, node->right);
        append_u8(&a->out, OPCODE_DIV);
        break;

        case NODE_OPER_MOD:
        assemble_node(a, node->left);
        assemble_node(a, node->right);
        append_u8(&a->out, OPCODE_MOD);
        break;

        case NODE_VALUE_INT:
        append_u8(&a->out, OPCODE_PUSHI);
        append_u64(&a->out, node->ival);
        break;

        case NODE_VALUE_FLOAT:
        append_u8 (&a->out, OPCODE_PUSHF);
        append_f64(&a->out, node->dval);
        break;

        case NODE_VALUE_STR:
        {
            int off = add_string_literal(a, node->sval);
            append_u8(&a->out, OPCODE_PUSHS);
            append_u32(&a->out, off);
            append_u32(&a->out, node->sval.len);
        }
        break;

        case NODE_VALUE_VAR:
        append_u8 (&a->out, OPCODE_PUSHV);
        append_u64(&a->out, node->ival);
        break;

        case NODE_VALUE_HTML:
        // TODO
        //assemble_html_node(a, node);
        break;

        case NODE_VALUE_ARRAY:
        {
            append_u8(&a->out, OPCODE_PUSHA);
            append_u32(&a->out, count_nodes(node->child));

            Node *child = node->child;
            while (child) {
                assemble_node(a, child);
                append_u8(&a->out, OPCODE_APPEND);
                child = child->next;
            }
        }
        break;

        case NODE_VALUE_MAP:
        {
            append_u8(&a->out, OPCODE_PUSHM);
            append_u64(&a->out, count_nodes(node->child));

            Node *child = node->child;
            while (child) {
                assemble_node(a, child);
                assemble_node(a, child->key);
                append_u8(&a->out, OPCODE_INSERT1);
                child = child->next;
            }
        }
        break;

        case NODE_SELECT:
        {
            Node *set = node->left;
            Node *key = node->right;

            assemble_node(a, set);
            assemble_node(a, key);
            append_u8(&a->out, OPCODE_SELECT);
        }
        break;

        case NODE_OPER_ASS:
        {
            Node *dst = node->left;
            Node *src = node->right;

            if (dst->type == NODE_VALUE_VAR) {

                int idx = find_variable(a, dst->sval);
                if (idx < 0) return;

                assemble_node(a, src);
                append_u8(&a->out, OPCODE_SETV);
                append_u8(&a->out, idx);

            } else if (dst->type == NODE_SELECT) {

                assemble_node(a, src);
                assemble_node(a, dst->left);
                assemble_node(a, dst->right);
                append_u8(&a->out, OPCODE_INSERT2);

            } else {

                assembler_report(a, "Assignment left side can't be assigned to");
                return;
            }
        }
        break;

        default:
        break;
    }
}

AssembleResult assemble(Node *root, char *errbuf, int errmax)
{
    Assembler a = {0, .errbuf=errbuf, .errmax=errmax, .errlen=0};
    assemble_node(&a, root);
    append_u8(&a.out, OPCODE_NOPE);

    Program p;
    // TODO

    return (AssembleResult) { p, a.errlen };
}
