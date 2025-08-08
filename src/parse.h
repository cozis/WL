#ifndef WL_PARSE_INCLUDED
#define WL_PARSE_INCLUDED

#ifndef WL_AMALGAMATION
#include "includes.h"
#include "basic.h"
#endif

typedef enum {
    NODE_FUNC_DECL,
    NODE_FUNC_ARG,
    NODE_FUNC_CALL,
    NODE_VAR_DECL,
    NODE_PRINT,
    NODE_BLOCK,
    NODE_GLOBAL_BLOCK,
    NODE_IFELSE,
    NODE_FOR,
    NODE_WHILE,
    NODE_INCLUDE,
    NODE_SELECT,
    NODE_NESTED,
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
    NODE_VALUE_NONE,
    NODE_VALUE_TRUE,
    NODE_VALUE_FALSE,
    NODE_VALUE_VAR,
    NODE_VALUE_SYSVAR,
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
    bool  no_body;

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

    String var_name;
    Node  *var_value;

    String include_path;
    Node*  include_next;
    Node*  include_root;
};

typedef struct {
    Node *node;
    Node *includes;
    int   errlen;
} ParseResult;

void print_node(Node *node);
ParseResult parse(String src, WL_Arena *a, char *errbuf, int errmax);

#endif // WL_PARSE_INCLUDED