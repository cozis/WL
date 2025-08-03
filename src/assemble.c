#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef WL_AMALGAMATION
#include "parse.h"
#include "assemble.h"
#endif

#define MAX_SCOPES  32
#define MAX_SYMBOLS 1024

typedef struct FunctionCall FunctionCall;
struct FunctionCall {
    FunctionCall *next;
    String        name;
    int           off;
};

typedef enum {
    SYMBOL_VAR,
    SYMBOL_FUNC,
} SymbolType;

typedef struct {
    SymbolType type;
    String     name;
    int        off;
} Symbol;

typedef enum {
    SCOPE_GLOBAL,
    SCOPE_FUNC,
    SCOPE_FOR,
    SCOPE_WHILE,
    SCOPE_IF,
    SCOPE_ELSE,
    SCOPE_BLOCK,
} ScopeType;

typedef struct {
    ScopeType     type;
    int           sym_base;
    int           max_vars;
    FunctionCall* calls;
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

    int num_syms;
    Symbol syms[MAX_SYMBOLS];

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

void patch_u32(OutputBuffer *out, int off, uint32_t x)
{
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

Scope *parent_scope(Assembler *a)
{
    assert(a->num_scopes > 0);

    int parent = a->num_scopes-1;
    while (a->scopes[parent].type != SCOPE_FUNC
        && a->scopes[parent].type != SCOPE_GLOBAL)
        parent--;

    Scope *scope = &a->scopes[parent];

    assert(scope->type == SCOPE_GLOBAL
        || scope->type == SCOPE_FUNC);

    return scope;
}

Symbol *find_symbol_in_local_scope(Assembler *a, String name)
{
    for (int i = a->num_syms-1; i >= a->scopes[a->num_scopes-1].sym_base; i--)
        if (streq(a->syms[i].name, name))
            return &a->syms[i];
    return NULL;
}

Symbol *find_symbol_in_function(Assembler *a, String name)
{
    Scope *scope = parent_scope(a);
    for (int i = a->num_syms-1; i >= scope->sym_base; i--)
        if (streq(a->syms[i].name, name))
            return &a->syms[i];
    return NULL;
}

int count_local_vars(Assembler *a)
{
    int n = 0;
    Scope *scope = parent_scope(a);
    for (int i = scope->sym_base; i < a->num_syms; i++)
        if (a->syms[i].type == SYMBOL_VAR)
            n++;
    return n;
}

int declare_variable(Assembler *a, String name)
{
    if (a->num_syms == MAX_SYMBOLS) {
        assembler_report(a, "Symbol limit reached");
        return -1;
    }

    if (find_symbol_in_local_scope(a, name)) {
        assembler_report(a, "Symbol '%.*s' already declared in this scope",
            name.len, name.ptr);
        return -1;
    }

    int off = count_local_vars(a);
    a->syms[a->num_syms++] = (Symbol) { SYMBOL_VAR, name, off };

    Scope *scope = parent_scope(a);

    if (scope->max_vars < off + 1)
        scope->max_vars = off + 1;

    return off;
}

int declare_function(Assembler *a, String name, int off)
{
    if (a->num_syms == MAX_SYMBOLS) {
        assembler_report(a, "Symbol limit reached");
        return -1;
    }

    if (find_symbol_in_local_scope(a, name)) {
        assembler_report(a, "Symbol '%.*s' already declared in this scope", name.len, name.ptr);
        return -1;
    }

    a->syms[a->num_syms++] = (Symbol) { SYMBOL_FUNC, name, off };
    return 0;
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
        case NODE_NESTED:
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

int push_scope(Assembler *a, ScopeType type)
{
    if (a->num_scopes == MAX_SCOPES) {
        assembler_report(a, "Scope limit reached");
        return -1;
    }
    Scope *scope = &a->scopes[a->num_scopes++];
    scope->type     = type;
    scope->sym_base = a->num_syms;
    scope->max_vars = 0;
    scope->calls    = NULL;
    return 0;
}

int pop_scope(Assembler *a)
{
    Scope *scope = &a->scopes[a->num_scopes-1];

    FunctionCall  *call = scope->calls;
    FunctionCall **prev = &scope->calls;
    while (call) {

        Symbol *sym = find_symbol_in_local_scope(a, call->name);

        if (sym == NULL) {
            prev = &call->next;
            call = call->next;
            continue;
        }

        if (sym->type != SYMBOL_FUNC) {
            assembler_report(a, "Symbol '%.*s' is not a function", call->name.len, call->name.ptr);
            return -1;
        }

        patch_u32(&a->out, call->off, sym->off);

        *prev = call->next;
        call = call->next;
    }

    if (scope->calls) {

        if (a->num_scopes == 1) {
            assembler_report(a, "Undefined function '%.*s'",
                scope->calls->name.len,
                scope->calls->name.ptr);
            return -1;
        }

        Scope *parent_scope = &a->scopes[a->num_scopes-2];
        *prev = parent_scope->calls;
        parent_scope->calls = scope->calls;
    }

    a->num_syms = scope->sym_base;
    a->num_scopes--;
    return 0;
}

void assemble_node(Assembler *a, Node *node)
{
    switch (node->type) {

        case NODE_PRINT:
        {
            assemble_node(a, node->left);
            append_u8(&a->out, OPCODE_PRINT);
            append_u8(&a->out, OPCODE_POP);
        }
        break;

        case NODE_FUNC_DECL:
        {
            append_u8(&a->out, OPCODE_JUMP);
            int p1 = append_u32(&a->out, 0);

            int ret = declare_function(a, node->func_name, current_offset(&a->out));
            if (ret < 0) return;

            ret = push_scope(a, SCOPE_FUNC);
            if (ret < 0) return;

            append_u8(&a->out, OPCODE_VARS);
            int p = append_u32(&a->out, 0);

            Node *args[32];
            int arg_count = 0;
            Node *arg = node->func_args;
            while (arg) {
                if (arg_count == COUNT(args)) {
                    assembler_report(a, "Argument limit reached");
                    return;
                }
                args[arg_count++] = arg;
                arg = arg->next;
            }

            for (int i = arg_count-1; i >= 0; i--) {

                int off = declare_variable(a, args[i]->sval);
                if (off < 0) return;

                append_u8(&a->out, OPCODE_SETV);
                append_u8(&a->out, off);
            }

            if (is_expr(node->func_body)) {
                assemble_node(a, node->func_body);
                append_u8(&a->out, OPCODE_RET);
            } else {
                assemble_node(a, node->func_body);
                append_u8(&a->out, OPCODE_PUSHN);
                append_u8(&a->out, OPCODE_RET);
            }

            patch_u32(&a->out, p, a->scopes[a->num_scopes-1].max_vars);

            ret = pop_scope(a);
            if (ret < 0) return;

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

            assert(func->type == NODE_VALUE_VAR);

            append_u8(&a->out, OPCODE_CALL);
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
        }
        break;

        case NODE_VAR_DECL:
        {
            int off = declare_variable(a, node->var_name);
            if (off < 0) return;

            if (node->var_value)
                assemble_node(a, node->var_value);
            else
                append_u8(&a->out, OPCODE_PUSHN);

            append_u8(&a->out, OPCODE_SETV);
            append_u8(&a->out, off);
        }
        break;

        case NODE_BLOCK:
        {
            int ret = push_scope(a, SCOPE_BLOCK);
            if (ret < 0) return;

            Node *stmt = node->left;
            while (stmt) {
                assemble_node(a, stmt);
                if (is_expr(stmt))
                    append_u8(&a->out, OPCODE_POP);
                stmt = stmt->next;
            }

            ret = pop_scope(a);
            if (ret < 0) return;
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

                int ret = push_scope(a, SCOPE_IF);
                if (ret < 0) return;

                assemble_node(a, node->left);

                ret = pop_scope(a);
                if (ret < 0) return;

                append_u8(&a->out, OPCODE_JUMP);
                int p2 = append_u32(&a->out, 0);

                patch_with_current_offset(&a->out, p1);

                ret = push_scope(a, SCOPE_ELSE);
                if (ret < 0) return;

                assemble_node(a, node->right);

                ret = pop_scope(a);
                if (ret < 0) return;

                patch_with_current_offset(&a->out, p2);

            } else {

                assemble_node(a, node->cond);

                append_u8(&a->out, OPCODE_JIFP);
                int p1 = append_u32(&a->out, 0);

                int ret = push_scope(a, SCOPE_IF);
                if (ret < 0) return;

                assemble_node(a, node->left);

                ret = pop_scope(a);
                if (ret < 0) return;

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

            int ret = push_scope(a, SCOPE_WHILE);
            if (ret < 0) return;

            assemble_node(a, node->left);

            ret = pop_scope(a);
            if (ret < 0) return;

            append_u8(&a->out, OPCODE_JUMP);
            append_u32(&a->out, start);

            patch_with_current_offset(&a->out, p);
        }
        break;

        case NODE_FOR:
        {
            assemble_node(a, node->for_set);

            // TODO

            int ret = push_scope(a, SCOPE_FOR);
            if (ret < 0) return;

            assemble_node(a, node->left);

            ret = pop_scope(a);
            if (ret < 0) return;
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
        {
            String name = node->sval;
            Symbol *sym = find_symbol_in_function(a, name);
            if (sym == NULL) {
                assembler_report(a, "Reference to undefined variable '%.*s'", name.len, name.ptr);
                return;
            }
            if (sym->type != SYMBOL_VAR) {
                assembler_report(a, "Symbol '%.*s' is not a variable", sym->name.len, sym->name.ptr);
                return;
            }
            append_u8(&a->out, OPCODE_PUSHV);
            append_u8(&a->out, sym->off);
        }
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
            append_u32(&a->out, count_nodes(node->child));

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

        case NODE_NESTED:
        assemble_node(a, node->left);
        break;

        case NODE_OPER_ASS:
        {
            Node *dst = node->left;
            Node *src = node->right;

            if (dst->type == NODE_VALUE_VAR) {

                String name = dst->sval;

                Symbol *sym = find_symbol_in_function(a, name);
                if (sym == NULL) {
                    assembler_report(a, "Undeclared variable '%.*s'", name.len, name.ptr);
                    return;
                }

                if (sym->type != SYMBOL_VAR) {
                    assembler_report(a, "Symbol '%.*s' can't be assigned to", name.len, name.ptr);
                    return;
                }

                assemble_node(a, src);
                append_u8(&a->out, OPCODE_SETV);
                append_u8(&a->out, sym->off);

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

typedef struct {
    uint32_t magic;
    uint32_t code_size;
    uint32_t data_size;
} Header;

AssembleResult assemble(Node *root, Arena *arena, char *errbuf, int errmax)
{
    Assembler a = {0, .a=arena, .errbuf=errbuf, .errmax=errmax, .errlen=0};

    int ret = push_scope(&a, SCOPE_GLOBAL);
    if (ret < 0)
        return (AssembleResult) { (Program) {0}, a.errlen };

    append_u8(&a.out, OPCODE_VARS);
    int p = append_u32(&a.out, 0);

    assemble_node(&a, root);
    append_u8(&a.out, OPCODE_EXIT);

    patch_u32(&a.out, p, a.scopes[a.num_scopes-1].max_vars);

    ret = pop_scope(&a);
    if (ret < 0)
        return (AssembleResult) { (Program) {0}, a.errlen };

    OutputBuffer out = {0};
    append_u32(&out, 0xFEEDBEEF);    // magic
    append_u32(&out, a.out.len);     // code size
    append_u32(&out, a.strings_len); // data size
    append_mem(&out, a.out.ptr, a.out.len);
    append_mem(&out, a.strings, a.strings_len);

    free(a.out.ptr);
    return (AssembleResult) { (Program) { out.ptr, out.len }, a.errlen };
}

void print_program(Program p)
{
    if (p.len < 3 * sizeof(uint32_t)) {
        printf("Invalid program");
        return;
    }

    uint32_t cur = 0;

    uint32_t magic;
    memcpy(&magic, p.ptr + cur, sizeof(uint32_t));
    cur += sizeof(uint32_t);

    uint32_t code_size;
    memcpy(&code_size, p.ptr + cur, sizeof(uint32_t));
    cur += sizeof(uint32_t);

    uint32_t data_size;
    memcpy(&data_size, p.ptr + cur, sizeof(uint32_t));
    cur += sizeof(uint32_t);

    if (3 * sizeof(uint32_t) + code_size + data_size != p.len) {
        printf("Invalid program");
        return;
    }

    printf("Program size is %d\n", p.len);

    while (cur < code_size + 3 * sizeof(uint32_t)) {

        printf("%" LLU ": ", cur - 3 * sizeof(uint32_t));

        switch (((uint8_t*) p.ptr)[cur++]) {

            case OPCODE_NOPE:
            printf("NOPE");
            break;

            case OPCODE_EXIT:
            printf("EXIT");
            break;

            case OPCODE_PUSHI:
            {
                uint64_t x;
                memcpy(&x, p.ptr + cur, sizeof(uint64_t));
                cur += sizeof(uint64_t);

                printf("PUSHI %" LLU, x);
            }
            break;

            case OPCODE_PUSHF:
            {
                double x;
                memcpy(&x, p.ptr + cur, sizeof(double));
                cur += sizeof(double);

                printf("PUSHF %lf", x);
            }
            break;

            case OPCODE_PUSHS:
            {
                uint32_t off;
                memcpy(&off, p.ptr + cur, sizeof(uint32_t));
                cur += sizeof(uint32_t);

                uint32_t len;
                memcpy(&len, p.ptr + cur, sizeof(uint32_t));
                cur += sizeof(uint32_t);

                printf("PUSHS \"%.*s\"", len, (char*) p.ptr + 3 * sizeof(uint32_t) + code_size + off);
            }
            break;

            case OPCODE_PUSHV:
            {
                uint8_t idx;
                memcpy(&idx, p.ptr + cur, sizeof(uint8_t));
                cur += sizeof(uint8_t);

                printf("PUSHV %u", idx);
            }
            break;

            case OPCODE_PUSHA:
            {
                uint32_t cap;
                memcpy(&cap, p.ptr + cur, sizeof(uint32_t));
                cur += sizeof(uint32_t);

                printf("PUSHA %u", cap);
            }
            break;

            case OPCODE_PUSHM:
            {
                uint32_t cap;
                memcpy(&cap, p.ptr + cur, sizeof(uint32_t));
                cur += sizeof(uint32_t);

                printf("PUSHM %u", cap);
            }
            break;

            case OPCODE_PUSHN:
            {
                printf("PUSHN");
            }
            break;

            case OPCODE_POP:
            printf("POP");
            break;

            case OPCODE_NEG:
            printf("NEG");
            break;

            case OPCODE_EQL:
            printf("EQL");
            break;

            case OPCODE_NQL:
            printf("NQL");
            break;

            case OPCODE_LSS:
            printf("LSS");
            break;

            case OPCODE_GRT:
            printf("GRT");
            break;

            case OPCODE_ADD:
            printf("ADD");
            break;

            case OPCODE_SUB:
            printf("SUB");
            break;

            case OPCODE_MUL:
            printf("MUL");
            break;

            case OPCODE_DIV:
            printf("DIV");
            break;

            case OPCODE_MOD:
            printf("MOD");
            break;

            case OPCODE_SETV:
            {
                uint8_t idx;
                memcpy(&idx, p.ptr + cur, sizeof(uint8_t));
                cur += sizeof(uint8_t);

                printf("SETV %u", idx);
            }
            break;

            case OPCODE_JUMP:
            {
                uint32_t off;
                memcpy(&off, p.ptr + cur, sizeof(uint32_t));
                cur += sizeof(uint32_t);

                printf("JUMP %u", off);
            }
            break;

            case OPCODE_JIFP:
            {
                uint32_t off;
                memcpy(&off, p.ptr + cur, sizeof(uint32_t));
                cur += sizeof(uint32_t);

                printf("JIFP %u", off);
            }
            break;

            case OPCODE_CALL:
            {
                uint8_t num;
                memcpy(&num, p.ptr + cur, sizeof(uint8_t));
                cur += sizeof(uint8_t);

                uint32_t off;
                memcpy(&off, p.ptr + cur, sizeof(uint32_t));
                cur += sizeof(uint32_t);

                printf("CALL %u %u", num, off);
            }
            break;

            case OPCODE_VARS:
            {
                uint32_t num;
                memcpy(&num, p.ptr + cur, sizeof(uint32_t));
                cur += sizeof(uint32_t);

                printf("VARS %u", num);
            }
            break;

            case OPCODE_RET:
            printf("RET");
            break;

            case OPCODE_APPEND:
            printf("APPEND");
            break;

            case OPCODE_INSERT1:
            printf("INSERT1");
            break;

            case OPCODE_INSERT2:
            printf("INSERT2");
            break;

            case OPCODE_SELECT:
            printf("SELECT");
            break;

            case OPCODE_PRINT:
            printf("PRINT");
            break;
        }

        printf("\n");
    }
}