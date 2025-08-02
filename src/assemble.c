#include "parse.h"

typedef enum {
    SCOPE_FUNC,
    SCOPE_FOR,
    SCOPE_WHILE,
    SCOPE_IF,
    SCOPE_ELSE,
} ScopeType;

typedef struct {
    Node *funcs;
    int   num_funcs;
    int   cap_funcs;
} Scope;

void assemble_node(Node *node)
{
    switch (node->type) {

        case NODE_FUNC_DECL:
        break;

        case NODE_FUNC_ARG:
        break;

        case NODE_FUNC_CALL:
        {
            Node *func = node->left;
            Node *args = node->right;

            if (func->type == NODE_VALUE_VAR) {
                // TODO
            }

        }
        break;

        case NODE_BLOCK:
        break;

        case NODE_IFELSE:
        break;

        case NODE_FOR:
        break;

        case NODE_WHILE:
        break;

        case NODE_SELECT:
        break;

        case NODE_OPER_POS:
        break;

        case NODE_OPER_NEG:
        break;

        case NODE_OPER_ASS:
        break;

        case NODE_OPER_EQL:
        break;

        case NODE_OPER_NQL:
        break;

        case NODE_OPER_LSS:
        break;

        case NODE_OPER_GRT:
        break;

        case NODE_OPER_ADD:
        break;

        case NODE_OPER_SUB:
        break;

        case NODE_OPER_MUL:
        break;

        case NODE_OPER_DIV:
        break;

        case NODE_OPER_MOD:
        break;

        case NODE_VALUE_INT:
        break;

        case NODE_VALUE_FLOAT:
        break;

        case NODE_VALUE_STR:
        break;

        case NODE_VALUE_VAR:
        break;

        case NODE_VALUE_HTML:
        break;

        case NODE_VALUE_ARRAY:
        break;

        case NODE_VALUE_MAP:
        break;

        case NODE_HTML_PARAM:
        break;
    }
}

void assemble(Node *root)
{

}
