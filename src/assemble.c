#include "parse.h"
#include "assemble.h"

#define MAX_SCOPES 32

typedef enum {
    SCOPE_FUNC,
    SCOPE_FOR,
    SCOPE_WHILE,
    SCOPE_IF,
    SCOPE_ELSE,
} ScopeType;

typedef struct {
    ScopeType type;
    // TODO
} Scope;

typedef struct {
    int depth;
    Scope scopes[MAX_SCOPES];
} Assembler;

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

void assemble_node(Node *node)
{
    switch (node->type) {

        case NODE_FUNC_DECL:
        {
            append_u8 (out, OPCODE_JUMP);
            append_u64(out, xxx);

            int arg_count = count_nodes(node->func_args);

            Node *preallocated[32];
            Node *args = preallocated;
            if (arg_count > COUNT(preallocated)) {
                args = malloc(arg_count * sizeof(Node*));
                if (args == NULL) {
                    // TODO
                }
            }

            for (int i = arg_count-1; i >= 0; i--) {
                append_u8 (out, OPCODE_ASS);
                append_u64(out, xxx);
            }

            if (is_expr(node->func_body)) {
                assemble_node(node->func_body);
                append_u8(out, OPCODE_RET);
            } else
                assemble_node(node->func_body);

            append_u8(out, OPCODE_NOPE);

            if (args != preallocated)
                free(args);
        }
        break;

        case NODE_FUNC_CALL:
        {
            Node *func = node->left;
            Node *args = node->right;

            int arg_count = 0;
            Node *arg = args;
            while (arg) {
                assemble_node(arg);
                arg_count++;
                arg = arg->next;
            }

            assemble_node(arg);
            append_u8(OPCODE_CALL);
            append_u32(arg_count);

            if (func->type == NODE_VALUE_VAR) {
                // TODO
            }

        }
        break;

        case NODE_BLOCK:
        {
            Node *stmt = node->child;
            while (stmt) {
                assemble_node(stmt);
                if (is_expr(stmt))
                    append_u8(out, OPCODE_POP);
                stmt = stmt->next;
            }
        }
        break;

        case NODE_IFELSE:
        {
            assemble_node(node->cond);

            append_u8 (out, OPCODE_JIFP);
            append_u32(out, xxx);

            assemble_node(node->left);

            if (node->right)
                assemble_node(node->right);
        }
        break;

        case NODE_FOR:
        {
            assemble_node(node->for_set);
            // TODO
        }
        break;

        case NODE_WHILE:
        break;

        case NODE_SELECT:
        break;

        case NODE_OPER_POS:
        assemble_node(node->left);
        break;

        case NODE_OPER_NEG:
        assemble_node(node->left);
        append(out, OPCODE_NEG);
        break;

        case NODE_OPER_ASS:
        break;

        case NODE_OPER_EQL:
        assemble_node(node->left);
        assemble_node(node->right);
        append(out, OPCODE_EQL);
        break;

        case NODE_OPER_NQL:
        assemble_node(node->left);
        assemble_node(node->right);
        append(out, OPCODE_NQL);
        break;

        case NODE_OPER_LSS:
        assemble_node(node->left);
        assemble_node(node->right);
        append(out, OPCODE_LSS);
        break;

        case NODE_OPER_GRT:
        assemble_node(node->left);
        assemble_node(node->right);
        append(out, OPCODE_GRT);
        break;

        case NODE_OPER_ADD:
        assemble_node(node->left);
        assemble_node(node->right);
        append(out, OPCODE_ADD);
        break;

        case NODE_OPER_SUB:
        assemble_node(node->left);
        assemble_node(node->right);
        append(out, OPCODE_SUB);
        break;

        case NODE_OPER_MUL:
        assemble_node(node->left);
        assemble_node(node->right);
        append(out, OPCODE_MUL);
        break;

        case NODE_OPER_DIV:
        assemble_node(node->left);
        assemble_node(node->right);
        append(out, OPCODE_DIV);
        break;

        case NODE_OPER_MOD:
        assemble_node(node->left);
        assemble_node(node->right);
        append(out, OPCODE_MOD);
        break;

        case NODE_VALUE_INT:
        append_u8 (out, OPCODE_PUSHI);
        append_u64(out, node->ival);
        break;

        case NODE_VALUE_FLOAT:
        append_u8 (out, OPCODE_PUSHF);
        append_f64(out, node->fval);
        break;

        case NODE_VALUE_STR:
        append_u8 (out, OPCODE_PUSHS);
        append_u64(out, xxx);
        break;

        case NODE_VALUE_VAR:
        append_u8 (out, OPCODE_PUSHV);
        append_u64(out, node->ival);
        break;

        case NODE_VALUE_HTML:
        TODO;
        break;

        case NODE_VALUE_ARRAY:
        append_u8 (out, OPCODE_PUSHA);
        append_u64(out, xxx);
        break;

        case NODE_VALUE_MAP:
        append_u8 (out, OPCODE_PUSHM);
        append_u64(out, xxx);
        break;
    }
}

AssembleResult assemble(Node *root)
{
    assemble_node(root);
}
