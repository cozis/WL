#ifndef WL_PARSE_INCLUDED
#define WL_PARSE_INCLUDED

#include <stdint.h>
#include "basic.h"

typedef enum {
    NODE_FUNC_DECL,
    NODE_FUNC_ARG,
    NODE_FUNC_CALL,
    NODE_BLOCK,
    NODE_IFELSE,
    NODE_FOR,
    NODE_WHILE,
    NODE_SELECT,
    NODE_OPER_POS,
    NODE_OPER_NEG,
    NODE_OPER_ASS,
    NODE_OPER_EQL,
    NODE_OPER_NQL,
    NODE_OPER_LSS,
    NODE_OPER_GRT,
    NODE_OPER_ADD,
    NODE_OPER_SUB,
    NODE_OPER_MUL,
    NODE_OPER_DIV,
    NODE_OPER_MOD,
    NODE_VALUE_INT,
    NODE_VALUE_FLOAT,
    NODE_VALUE_STR,
    NODE_VALUE_VAR,
    NODE_VALUE_HTML,
    NODE_VALUE_ARRAY,
    NODE_VALUE_MAP,
    NODE_HTML_PARAM,
} NodeType;

typedef struct Node Node;
struct Node {
    NodeType type;
    Node *next;

    Node *key;

    Node *left;
    Node *right;

    uint64_t ival;
    double   dval;
    String   sval;

    Node *params;
    Node *child;

    Node *cond;

    String tagname;
    String attr_name;
    Node  *attr_value;

    String for_var1;
    String for_var2;
    Node *for_set;

    String func_name;
    Node  *func_args;
    Node  *func_body;
};

typedef struct {
    Node *node;
    int   errlen;
} ParseResult;

void print_node(Node *node);
ParseResult parse(String src, Arena *a, char *errbuf, int errmax);

#endif // WL_PARSE_INCLUDED