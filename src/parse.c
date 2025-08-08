
#ifndef WL_AMALGAMATION
#include "parse.h"
#endif

typedef struct {
    char *src;
    int   len;
    int   cur;
} Scanner;

typedef enum {
    TOKEN_END,
    TOKEN_ERROR,
    TOKEN_IDENT,
    TOKEN_KWORD_IF,
    TOKEN_KWORD_ELSE,
    TOKEN_KWORD_WHILE,
    TOKEN_KWORD_FOR,
    TOKEN_KWORD_IN,
    TOKEN_KWORD_FUN,
    TOKEN_KWORD_LET,
    TOKEN_KWORD_PRINT,
    TOKEN_KWORD_NONE,
    TOKEN_KWORD_TRUE,
    TOKEN_KWORD_FALSE,
    TOKEN_KWORD_INCLUDE,
    TOKEN_VALUE_FLOAT,
    TOKEN_VALUE_INT,
    TOKEN_VALUE_STR,
    TOKEN_OPER_EQL,
    TOKEN_OPER_NQL,
    TOKEN_OPER_LSS,
    TOKEN_OPER_GRT,
    TOKEN_OPER_ADD,
    TOKEN_OPER_SUB,
    TOKEN_OPER_MUL,
    TOKEN_OPER_DIV,
    TOKEN_OPER_MOD,
    TOKEN_OPER_ASS,
    TOKEN_PAREN_OPEN,
    TOKEN_PAREN_CLOSE,
    TOKEN_BRACKET_OPEN,
    TOKEN_BRACKET_CLOSE,
    TOKEN_CURLY_OPEN,
    TOKEN_CURLY_CLOSE,
    TOKEN_DOT,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_DOLLAR,
    TOKEN_NEWLINE,
} TokType;

typedef struct {
    TokType type;
    union {
        int64_t  ival;
        uint64_t uval;
        double   dval;
        String   sval;
    };
} Token;

typedef struct {
    Scanner   s;
    WL_Arena* a;
    char*     errbuf;
    int       errmax;
    int       errlen;
    Node*     include_head;
    Node**    include_tail;
} Parser;

bool consume_str(Scanner *s, String x)
{
    if (x.len == 0)
        return false;

    if (x.len > s->len - s->cur)
        return false;

    for (int i = 0; i < x.len; i++)
        if (s->src[s->cur+i] != x.ptr[i])
            return false;

    s->cur += x.len;
    return true;
}

String tok2str(Token token, char *buf, int max)
{
    switch (token.type) {

        case TOKEN_END:
        return S("EOF");

        case TOKEN_ERROR:
        return S("ERROR");

        case TOKEN_IDENT:
        {
            int len = snprintf(buf, max, "%.*s", token.sval.len, token.sval.ptr);
            return (String) { buf, len };
        }
        break;

        case TOKEN_KWORD_IF: return S("if");
        case TOKEN_KWORD_ELSE: return S("else");
        case TOKEN_KWORD_WHILE: return S("while");
        case TOKEN_KWORD_FOR: return S("for");
        case TOKEN_KWORD_IN: return S("in");
        case TOKEN_KWORD_FUN: return S("fun");
        case TOKEN_KWORD_LET: return S("let");
        case TOKEN_KWORD_PRINT: return S("print");
        case TOKEN_KWORD_NONE: return S("none");
        case TOKEN_KWORD_TRUE: return S("true");
        case TOKEN_KWORD_FALSE: return S("false");
        case TOKEN_KWORD_INCLUDE: return S("include");

        case TOKEN_VALUE_FLOAT:
        {
            int len = snprintf(buf, max, "%lf", token.dval);
            return (String) { buf, len };
        }
        break;

        case TOKEN_VALUE_INT:
        {
            int len = snprintf(buf, max, "%" LLU, token.uval);
            return (String) { buf, len };
        }
        break;

        case TOKEN_VALUE_STR:
        {
            int len = snprintf(buf, max, "\"%.*s\"", token.sval.len, token.sval.ptr);
            return (String) { buf, len };
        }
        break;

        case TOKEN_OPER_ASS: return S("==");
        case TOKEN_OPER_EQL: return S("==");
        case TOKEN_OPER_NQL: return S("!=");
        case TOKEN_OPER_LSS: return S("<");
        case TOKEN_OPER_GRT: return S(">");
        case TOKEN_OPER_ADD: return S("+");
        case TOKEN_OPER_SUB: return S("-");
        case TOKEN_OPER_MUL: return S("*");
        case TOKEN_OPER_DIV: return S("/");
        case TOKEN_OPER_MOD: return S("%");

        case TOKEN_PAREN_OPEN: return S("(");
        case TOKEN_PAREN_CLOSE: return S(")");

        case TOKEN_BRACKET_OPEN: return S("[");
        case TOKEN_BRACKET_CLOSE: return S("]");

        case TOKEN_CURLY_OPEN: return S("{");
        case TOKEN_CURLY_CLOSE: return S("}");

        case TOKEN_DOT: return S(".");
        case TOKEN_COMMA: return S(",");
        case TOKEN_COLON: return S(":");
        case TOKEN_DOLLAR: return S("$");

        case TOKEN_NEWLINE: return S("\\n");
    }

    return S("???");
}

void parser_report(Parser *p, char *fmt, ...)
{
    if (p->errmax == 0 || p->errlen > 0)
        return;

    int line = 1;
    int cur = 0;
    while (cur < p->s.cur) {
        if (p->s.src[cur] == '\n')
            line++;
        cur++;
    }

    int len = snprintf(p->errbuf, p->errmax, "Error (line %d): ", line);
    if (len < 0) {
        // TODO
    }

    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(p->errbuf + len, p->errmax - len, fmt, args);
    va_end(args);
    if (ret < 0) {
        // TODO
    }
    len += ret;

    p->errlen = len;
}

Node *alloc_node(Parser *p)
{
    Node *n = alloc(p->a, sizeof(Node), _Alignof(Node));
    if (n == NULL) {
        parser_report(p, "Out of memory");
        return NULL;
    }

    return n;
}

Token next_token(Parser *p)
{
    for (;;) {
        while (p->s.cur < p->s.len && is_space(p->s.src[p->s.cur]))
            p->s.cur++;

        if (!consume_str(&p->s, S("<!--")))
            break;

        while (p->s.cur < p->s.len) {
            if (consume_str(&p->s, S("-->")))
                break;
            p->s.cur++;
        }
    }

    if (p->s.cur == p->s.len)
        return (Token) { .type=TOKEN_END };
    char c = p->s.src[p->s.cur];

    if (is_alpha(c) || c == '_') {

        int start = p->s.cur;
        do
            p->s.cur++;
        while (p->s.cur < p->s.len && (is_alpha(p->s.src[p->s.cur]) || is_digit(p->s.src[p->s.cur]) || p->s.src[p->s.cur] == '_'));

        String kword = {
            p->s.src + start,
            p->s.cur - start
        };

        if (streq(kword, S("if")))    return (Token) { .type=TOKEN_KWORD_IF };
        if (streq(kword, S("else")))  return (Token) { .type=TOKEN_KWORD_ELSE };
        if (streq(kword, S("while"))) return (Token) { .type=TOKEN_KWORD_WHILE };
        if (streq(kword, S("for")))   return (Token) { .type=TOKEN_KWORD_FOR };
        if (streq(kword, S("in")))    return (Token) { .type=TOKEN_KWORD_IN };
        if (streq(kword, S("fun")))   return (Token) { .type=TOKEN_KWORD_FUN };
        if (streq(kword, S("let")))   return (Token) { .type=TOKEN_KWORD_LET };
        if (streq(kword, S("print"))) return (Token) { .type=TOKEN_KWORD_PRINT };
        if (streq(kword, S("none")))  return (Token) { .type=TOKEN_KWORD_NONE };
        if (streq(kword, S("true")))  return (Token) { .type=TOKEN_KWORD_TRUE };
        if (streq(kword, S("false"))) return (Token) { .type=TOKEN_KWORD_FALSE };
        if (streq(kword, S("include"))) return (Token) { .type=TOKEN_KWORD_INCLUDE };

        kword = copystr(kword, p->a);
        if (kword.len == 0) {
            parser_report(p, "Out of memory");
            return (Token) { .type=TOKEN_ERROR };
        }

        return (Token) { .type=TOKEN_IDENT, .sval=kword };
    }

    if (is_digit(c)) {

        int peek = p->s.cur;
        do
            peek++;
        while (peek < p->s.len && is_digit(p->s.src[peek]));

        if (p->s.len - peek > 1 && p->s.src[peek] == '.' && is_digit(p->s.src[peek+1])) {

            double buf = 0;
            do {
                int d = p->s.src[p->s.cur++] - '0';
                buf = buf * 10 + d;
            } while (p->s.cur < p->s.len && p->s.src[p->s.cur] != '.');

            p->s.cur++;

            double q = 1;
            do {
                int d = p->s.src[p->s.cur++] - '0';
                q /= 10;
                buf += q * d;
            } while (p->s.cur < p->s.len && is_digit(p->s.src[p->s.cur]));

            return (Token) { .type=TOKEN_VALUE_FLOAT, .dval=buf };

        } else {

            uint64_t buf = 0;
            do {
                int d = p->s.src[p->s.cur++] - '0';
                if (buf > (UINT64_MAX - d) / 10) {
                    parser_report(p, "Integer literal overflow");
                    return (Token) { .type=TOKEN_ERROR };
                }
                buf = buf * 10 + d;
            } while (p->s.cur < p->s.len && is_digit(p->s.src[p->s.cur]));

            return (Token) { .type=TOKEN_VALUE_INT, .uval=buf };
        }
    }

    if (c == '\'' || c == '"') {

        char f = c;
        p->s.cur++;

        char *buf = NULL;
        int   len = 0;

        for (;;) {

            int substr_off = p->s.cur;

            while (p->s.cur < p->s.len && is_printable(p->s.src[p->s.cur]) && p->s.src[p->s.cur] != f && p->s.src[p->s.cur] != '\\')
                p->s.cur++;

            int substr_len = p->s.cur - substr_off;

            if (buf == NULL)
                buf = alloc(p->a, substr_len+1, 1);
            else
                if (!grow_alloc(p->a, buf, len + substr_len+1))
                    buf = NULL;

            if (buf == NULL) {
                parser_report(p, "Out of memory");
                return (Token) { .type=TOKEN_ERROR };
            }

            if (substr_len > 0) {
                memcpy(
                    buf + len,
                    p->s.src + substr_off,
                    p->s.cur - substr_off
                );
                len += substr_len;
            }

            if (p->s.cur == p->s.len) {
                parser_report(p, "String literal wasn't closed");
                return (Token) { .type=TOKEN_ERROR };
            }

            if (!is_printable(p->s.src[p->s.cur])) {
                parser_report(p, "Invalid byte in string literal");
                return (Token) { .type=TOKEN_ERROR };
            }

            if (p->s.src[p->s.cur] == f)
                break;

            p->s.cur++;
            if (p->s.cur == p->s.len) {
                parser_report(p, "Missing special character after escape character \\");
                return (Token) { .type=TOKEN_ERROR };
            }

            switch (p->s.src[p->s.cur]) {
                case 'n':  buf[len++] = '\n'; break;
                case 't':  buf[len++] = '\t'; break;
                case 'r':  buf[len++] = '\r'; break;
                case '"':  buf[len++] = '"';  break;
                case '\'': buf[len++] = '\''; break;
                case '\\': buf[len++] = '\\'; break;

                case 'x':
                {
                    if (p->s.len - p->s.cur < 3
                        || !is_hex_digit(p->s.src[p->s.cur+1])
                        || !is_hex_digit(p->s.src[p->s.cur+2]))
                        return (Token) { .type=TOKEN_ERROR };
                    buf[len++]
                        = (hex_digit_to_int(p->s.src[p->s.cur+1]) << 4)
                        | (hex_digit_to_int(p->s.src[p->s.cur+2]) << 0);
                    p->s.cur += 2;
                }
                break;

                default:
                parser_report(p, "Invalid character after escape character \\");
                return (Token) { .type=TOKEN_ERROR };
            }

            p->s.cur++;
        }

        p->s.cur++;
        return (Token) { .type=TOKEN_VALUE_STR, .sval=(String) { .ptr=buf, .len=len } };
    }

    if (consume_str(&p->s, S("=="))) return (Token) { .type=TOKEN_OPER_EQL };
    if (consume_str(&p->s, S("!="))) return (Token) { .type=TOKEN_OPER_NQL };
    if (consume_str(&p->s, S("<")))  return (Token) { .type=TOKEN_OPER_LSS };
    if (consume_str(&p->s, S(">")))  return (Token) { .type=TOKEN_OPER_GRT };
    if (consume_str(&p->s, S("+")))  return (Token) { .type=TOKEN_OPER_ADD };
    if (consume_str(&p->s, S("-")))  return (Token) { .type=TOKEN_OPER_SUB };
    if (consume_str(&p->s, S("*")))  return (Token) { .type=TOKEN_OPER_MUL };
    if (consume_str(&p->s, S("/")))  return (Token) { .type=TOKEN_OPER_DIV };
    if (consume_str(&p->s, S("%")))  return (Token) { .type=TOKEN_OPER_MOD };
    if (consume_str(&p->s, S("=")))  return (Token) { .type=TOKEN_OPER_ASS };

    if (consume_str(&p->s, S("(")))  return (Token) { .type=TOKEN_PAREN_OPEN };
    if (consume_str(&p->s, S(")")))  return (Token) { .type=TOKEN_PAREN_CLOSE };
    if (consume_str(&p->s, S("[")))  return (Token) { .type=TOKEN_BRACKET_OPEN };
    if (consume_str(&p->s, S("]")))  return (Token) { .type=TOKEN_BRACKET_CLOSE };
    if (consume_str(&p->s, S("{")))  return (Token) { .type=TOKEN_CURLY_OPEN };
    if (consume_str(&p->s, S("}")))  return (Token) { .type=TOKEN_CURLY_CLOSE };
    if (consume_str(&p->s, S(".")))  return (Token) { .type=TOKEN_DOT };
    if (consume_str(&p->s, S(",")))  return (Token) { .type=TOKEN_COMMA };
    if (consume_str(&p->s, S(":")))  return (Token) { .type=TOKEN_COLON };
    if (consume_str(&p->s, S("$")))  return (Token) { .type=TOKEN_DOLLAR };

    parser_report(p, "Invalid character '%c'", c);
    return (Token) { .type=TOKEN_ERROR };
}

Token next_token_or_newline(Parser *p)
{
    int peek = p->s.cur;
    while (peek < p->s.len && is_space(p->s.src[peek]) && p->s.src[peek] != '\n')
        peek++;

    if (peek < p->s.len && p->s.src[peek] == '\n') {
        p->s.cur = peek+1;
        return (Token) { .type=TOKEN_NEWLINE };
    }

    return next_token(p);
}

enum {
    IGNORE_GRT = 1 << 0,
    IGNORE_LSS = 1 << 1,
    IGNORE_DIV = 1 << 2,
};

Node *parse_stmt(Parser *p, int opflags);
Node *parse_expr(Parser *p, int opflags);

Node *parse_html(Parser *p)
{
    // NOTE: The first < was already consumed
    
    Token t = next_token(p);
    if (t.type != TOKEN_IDENT) {
        char buf[1<<8];
        String ts = tok2str(t, buf, COUNT(buf));
        parser_report(p, "HTML tag doesn't start with a name (got '%.*s' instead)", ts.len, ts.ptr);
        return NULL;
    }
    String tagname = t.sval;

    Node *param_head;
    Node **param_tail = &param_head;

    bool no_body = false;
    for (;;) {

        String attr_name;
        Node  *attr_value;

        t = next_token(p);

        if (t.type == TOKEN_OPER_GRT)
            break;

        if (t.type == TOKEN_OPER_DIV) {
            t = next_token(p);
            if (t.type != TOKEN_OPER_GRT) {
                parser_report(p, "Invalid token '/' inside an HTML tag");
                return NULL;
            }
            no_body = true;
            break;
        }

        if (t.type != TOKEN_IDENT) {
            parser_report(p, "Invalid token inside HTML tag");
            return NULL;
        }
        attr_name = t.sval;

        Scanner saved = p->s;
        t = next_token(p);
        if (t.type == TOKEN_OPER_ASS) {

            attr_value = parse_expr(p, IGNORE_GRT | IGNORE_DIV);
            if (attr_value == NULL)
                return NULL;

        } else {
            p->s = saved;
            attr_value = NULL;
        }

        Node *child = alloc_node(p);
        if (child == NULL)
            return NULL;

        child->type = NODE_HTML_PARAM;
        child->attr_name  = attr_name;
        child->attr_value = attr_value;

        *param_tail = child;
        param_tail = &child->next;
    }

    *param_tail = NULL;

    Node *head;
    Node **tail = &head;

    if (!no_body) for (;;) {

        for (;;) {

            int off = p->s.cur;

            for (;;) {

                while (p->s.cur < p->s.len && p->s.src[p->s.cur] != '<' && p->s.src[p->s.cur] != '\\')
                    p->s.cur++;

                if (!consume_str(&p->s, S("<!--")))
                    break;

                while (p->s.cur < p->s.len) {
                    if (consume_str(&p->s, S("-->")))
                        break;
                    p->s.cur++;
                }
            }

            if (p->s.cur > off) {

                Node *child = alloc_node(p);
                if (child == NULL)
                    return NULL;

                child->type = NODE_VALUE_STR;
                child->sval = (String) { p->s.src + off, p->s.cur - off };

                *tail = child;
                tail = &child->next;
            }

            if (p->s.cur == p->s.len || p->s.src[p->s.cur] == '<')
                break;

            p->s.cur++; // Consume "\"

            {
                Node *child = parse_stmt(p, IGNORE_LSS);
                if (child == NULL)
                    return NULL;

                *tail = child;
                tail = &child->next;
            }
        }

        if (p->s.cur == p->s.len) {
            parser_report(p, "Missing closing HTML tag");
            return NULL;
        }
        p->s.cur++; // Consume <

        Scanner saved = p->s;
        t = next_token(p);
        if (t.type == TOKEN_OPER_DIV) {
            t = next_token(p);
            if (t.type == TOKEN_IDENT && streqcase(t.sval, tagname)) {
                t = next_token(p);
                if (t.type != TOKEN_OPER_GRT) {
                    parser_report(p, "Unexpected token in closing HTML tag");
                    return NULL;
                }
                break;
            }
        }

        p->s = saved;

        Node *child = parse_html(p);
        if (child == NULL)
            return NULL;

        *tail = child;
        tail = &child->next;
    }

    *tail = NULL;

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_VALUE_HTML;
    parent->tagname = tagname;
    parent->params = param_head;
    parent->child  = head;
    parent->no_body = no_body;

    return parent;
}

Node *parse_array(Parser *p)
{
    // Left bracket already consumed

    Node *head;
    Node **tail = &head;

    Scanner saved = p->s;
    Token t = next_token(p);
    if (t.type != TOKEN_BRACKET_CLOSE) {

        p->s = saved;

        for (;;) {

            Node *child = parse_expr(p, 0);
            if (child == NULL)
                return NULL;

            *tail = child;
            tail = &child->next;

            saved = p->s;
            t = next_token(p);
            if (t.type == TOKEN_COMMA) {
                saved = p->s;
                t = next_token(p);
            }

            if (t.type == TOKEN_BRACKET_CLOSE)
                break;

            p->s = saved;
        }
    }

    *tail = NULL;

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_VALUE_ARRAY;
    parent->child  = head;

    return parent;
}

Node *parse_map(Parser *p)
{
    // Left bracket already consumed

    Node *head;
    Node **tail = &head;

    Scanner saved = p->s;
    Token t = next_token(p);
    if (t.type != TOKEN_CURLY_CLOSE) {

        p->s = saved;

        for (;;) {

            Node *key;

            saved = p->s;
            t = next_token(p);
            if (t.type == TOKEN_IDENT) {
   
                key = alloc_node(p);
                if (key == NULL)
                    return NULL;

                key->type = NODE_VALUE_STR;
                key->sval = t.sval;

            } else {

                p->s = saved;
                key = parse_expr(p, 0);
                if (key == NULL)
                    return NULL;
            }

            t = next_token(p);
            if (t.type != TOKEN_COLON) {
                parser_report(p, "Missing ':' after key inside map literal");
                return NULL;
            }

            Node *child = parse_expr(p, 0);
            if (child == NULL)
                return NULL;
            child->key = key;

            *tail = child;
            tail = &child->next;

            saved = p->s;
            t = next_token(p);
            if (t.type == TOKEN_COMMA) {
                saved = p->s;
                t = next_token(p);
            }

            if (t.type == TOKEN_CURLY_CLOSE)
                break;

            p->s = saved;
        }
    }

    *tail = NULL;

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_VALUE_MAP;
    parent->child  = head;

    return parent;
}

int precedence(Token t, int flags)
{
    switch (t.type) {

        case TOKEN_OPER_ASS:
        return 1;

        case TOKEN_OPER_EQL:
        case TOKEN_OPER_NQL:
        return 2;

        case TOKEN_OPER_LSS:
        if (flags & IGNORE_LSS)
            return -1;
        return 2;

        case TOKEN_OPER_GRT:
        if (flags & IGNORE_GRT)
            return -1;
        return 2;

        case TOKEN_OPER_ADD:
        case TOKEN_OPER_SUB:
        return 3;

        case TOKEN_OPER_MUL:
        case TOKEN_OPER_MOD:
        return 4;

        case TOKEN_OPER_DIV:
        if (flags & IGNORE_DIV)
            return -1;
        return 4;

        default:
        break;
    }

    return -1;
}

bool right_associative(Token t)
{
    return t.type == TOKEN_OPER_ASS;
}

Node *parse_atom(Parser *p)
{
    Token t = next_token(p);

    Node *ret;
    switch (t.type) {
        case TOKEN_OPER_ADD:
        {
            Node *child = parse_atom(p);
            if (child == NULL)
                return NULL;

            Node *parent = alloc_node(p);
            if (parent == NULL)
                return NULL;

            parent->type = NODE_OPER_POS;
            parent->left = child;

            ret = parent;
        }
        break;

        case TOKEN_OPER_SUB:
        {
            Node *child = parse_atom(p);
            if (child == NULL)
                return NULL;

            Node *parent = alloc_node(p);
            if (parent == NULL)
                return NULL;

            parent->type = NODE_OPER_NEG;
            parent->left = child;

            ret = parent;
        }
        break;

        case TOKEN_IDENT:
        {
            Node *node = alloc_node(p);
            if (node == NULL)
                return NULL;

            node->type = NODE_VALUE_VAR;
            node->sval = t.sval;

            ret = node;
        }
        break;

        case TOKEN_VALUE_INT:
        {
            Node *node = alloc_node(p);
            if (node == NULL)
                return NULL;

            node->type = NODE_VALUE_INT;
            node->ival = t.uval;

            ret = node;
        }
        break;

        case TOKEN_VALUE_FLOAT:
        {
            Node *node = alloc_node(p);
            if (node == NULL)
                return NULL;

            node->type = NODE_VALUE_FLOAT;
            node->dval = t.dval;

            ret = node;
        }
        break;

        case TOKEN_VALUE_STR:
        {
            Node *node = alloc_node(p);
            if (node == NULL)
                return NULL;

            node->type = NODE_VALUE_STR;
            node->sval = t.sval;

            ret = node;
        }
        break;

        case TOKEN_KWORD_NONE:
        {
            Node *node = alloc_node(p);
            if (node == NULL)
                return NULL;

            node->type = NODE_VALUE_NONE;
            node->sval = t.sval;

            ret = node;
        }
        break;

        case TOKEN_KWORD_TRUE:
        {
            Node *node = alloc_node(p);
            if (node == NULL)
                return NULL;

            node->type = NODE_VALUE_TRUE;
            node->sval = t.sval;

            ret = node;
        }
        break;
        case TOKEN_KWORD_FALSE:
        {
            Node *node = alloc_node(p);
            if (node == NULL)
                return NULL;

            node->type = NODE_VALUE_FALSE;
            node->sval = t.sval;

            ret = node;
        }
        break;

        case TOKEN_OPER_LSS:
        {
            Node *node = parse_html(p);
            if (node == NULL)
                return NULL;

            ret = node;
        }
        break;

        case TOKEN_PAREN_OPEN:
        {
            Node *node = parse_expr(p, 0);
            if (node == NULL)
                return NULL;

            Token t = next_token(p);
            if (t.type != TOKEN_PAREN_CLOSE) {
                parser_report(p, "Missing ')' after expression");
                return NULL;
            }

            Node *parent = alloc_node(p);
            if (parent == NULL)
                return NULL;

            parent->type = NODE_NESTED;
            parent->left = node;

            ret = parent;
        }
        break;

        case TOKEN_BRACKET_OPEN:
        {
            Node *node = parse_array(p);
            if (node == NULL)
                return NULL;

            ret = node;
        }
        break;

        case TOKEN_CURLY_OPEN:
        {
            Node *node = parse_map(p);
            if (node == NULL)
                return NULL;

            ret = node;
        }
        break;

        case TOKEN_DOLLAR:
        {
            t = next_token(p);
            if (t.type != TOKEN_IDENT) {
                parser_report(p, "Missing identifier after '$'");
                return NULL;
            }

            Node *node = alloc_node(p);
            if (node == NULL)
                return NULL;

            node->type = NODE_VALUE_SYSVAR;
            node->sval = t.sval;

            ret = node;
        }
        break;

        default:
        {
            char buf[1<<8];
            String str = tok2str(t, buf, COUNT(buf));
            parser_report(p, "Invalid token \'%.*s\' inside expression", str.len, str.ptr);
        }
        return NULL;
    }

    for (;;) {
        Scanner saved = p->s;
        t = next_token(p);
        if (t.type == TOKEN_DOT) {

            t = next_token(p);
            if (t.type != TOKEN_IDENT) {
                parser_report(p, "Invalid token after '.' where an identifier was expected");
                return NULL;
            }

            Node *child = alloc_node(p);
            if (child == NULL)
                return NULL;

            child->type = NODE_VALUE_STR;
            child->sval = t.sval;

            Node *parent = alloc_node(p);
            if (parent == NULL)
                return NULL;

            parent->type = NODE_SELECT;
            parent->left = ret;
            parent->right = child;

            ret = parent;

        } else if (t.type == TOKEN_BRACKET_OPEN) {

            Node *child = parse_expr(p, 0);
            if (child == NULL)
                return NULL;

            t = next_token(p);
            if (t.type != TOKEN_BRACKET_CLOSE) {
                parser_report(p, "Missing token ']'");
                return NULL;
            }

            Node *parent = alloc_node(p);
            if (parent == NULL)
                return NULL;

            parent->type = NODE_SELECT;
            parent->left = ret;
            parent->right = child;

            ret = parent;

        } else if (t.type == TOKEN_PAREN_OPEN && (ret->type == NODE_VALUE_VAR || ret->type == NODE_VALUE_SYSVAR)) {

            Node *arg_head;
            Node **arg_tail = &arg_head;

            Scanner saved = p->s;
            t = next_token(p);
            if (t.type != TOKEN_PAREN_CLOSE) {

                p->s = saved;

                for (;;) {

                    Node *argval = parse_expr(p, 0);
                    if (argval == NULL)
                        return NULL;

                    *arg_tail = argval;
                    arg_tail = &argval->next;

                    t = next_token(p);
                    if (t.type == TOKEN_PAREN_CLOSE)
                        break;

                    if (t.type != TOKEN_COMMA) {
                        parser_report(p, "Expected ',' after argument in function call");
                        return NULL;
                    }
                }
            }

            *arg_tail = NULL;

            Node *parent = alloc_node(p);
            if (parent == NULL)
                return NULL;

            parent->type = NODE_FUNC_CALL;
            parent->left = ret;
            parent->right = arg_head;

            ret = parent;

        } else {
            p->s = saved;
            break;
        }
    }

    return ret;
}

Node *parse_expr_inner(Parser *p, Node *left, int min_prec, int flags)
{
    for (;;) {

        Scanner saved = p->s;
        Token t1 = next_token_or_newline(p);
        if (precedence(t1, flags) < min_prec) {
            p->s = saved;
           break;
        }

        Node *right = parse_atom(p);
        if (right == NULL)
            return NULL;

        for (;;) {

            saved = p->s;
            Token t2 = next_token_or_newline(p);
            int p1 = precedence(t1, flags);
            int p2 = precedence(t2, flags);
            p->s = saved;

            if (p2 < 0)
                break;

            if (p2 <= p1 && (p1 != p2 || !right_associative(t2)))
                break;

            right = parse_expr_inner(p, right, p1 + (p2 > p1), flags);
            if (right == NULL)
                return NULL;
        }

        Node *parent = alloc_node(p);
        if (parent == NULL)
            return NULL;

        parent->left = left;
        parent->right = right;

        switch (t1.type) {
            case TOKEN_OPER_ASS: parent->type = NODE_OPER_ASS; break;
            case TOKEN_OPER_EQL: parent->type = NODE_OPER_EQL; break;
            case TOKEN_OPER_NQL: parent->type = NODE_OPER_NQL; break;
            case TOKEN_OPER_LSS: parent->type = NODE_OPER_LSS; break;
            case TOKEN_OPER_GRT: parent->type = NODE_OPER_GRT; break;
            case TOKEN_OPER_ADD: parent->type = NODE_OPER_ADD; break;
            case TOKEN_OPER_SUB: parent->type = NODE_OPER_SUB; break;
            case TOKEN_OPER_MUL: parent->type = NODE_OPER_MUL; break;
            case TOKEN_OPER_DIV: parent->type = NODE_OPER_DIV; break;
            case TOKEN_OPER_MOD: parent->type = NODE_OPER_MOD; break;
            default: 
            parser_report(p, "Operator not implemented");
            return NULL;
        }

        left = parent;
    }

    return left;
}

Node *parse_expr(Parser *p, int flags)
{
    Node *left = parse_atom(p);
    if (left == NULL)
        return NULL;

    return parse_expr_inner(p, left, 0, flags);
}

Node *parse_expr_stmt(Parser *p, int opflags)
{
    Node *e = parse_expr(p, opflags);
    if (e == NULL)
        return NULL;

    return e;
}

Node *parse_ifelse_stmt(Parser *p, int opflags)
{
    Token t = next_token(p);
    if (t.type != TOKEN_KWORD_IF) {
        parser_report(p, "Missing 'if' keyword before if statement");
        return NULL;
    }

    Node *cond = parse_expr(p, 0);
    if (cond == NULL)
        return NULL;

    t = next_token(p);
    if (t.type != TOKEN_COLON) {
        parser_report(p, "Missing ':' after if condition");
        return NULL;
    }

    Node *if_stmt = parse_stmt(p, opflags);
    if (if_stmt == NULL)
        return NULL;

    Scanner saved = p->s;
    t = next_token(p);

    Node *else_stmt = NULL;
    if (t.type == TOKEN_KWORD_ELSE) {

        else_stmt = parse_stmt(p, opflags);
        if (else_stmt == NULL)
            return NULL;

    } else {
        p->s = saved;
    }

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_IFELSE;
    parent->left = if_stmt;
    parent->right = else_stmt;
    parent->cond = cond;

    return parent;
}

Node *parse_for_stmt(Parser *p, int opflags)
{
    Token t = next_token(p);
    if (t.type != TOKEN_KWORD_FOR) {
        parser_report(p, "Missing 'for' keyword at the start of a for statement");
        return NULL;
    }

    t = next_token(p);
    if (t.type != TOKEN_IDENT) {
        parser_report(p, "Missing iteraion variable name in for statement");
        return NULL;
    }
    String var1 = t.sval;

    t = next_token(p);

    String var2 = S("");
    if (t.type == TOKEN_COMMA) {

        t = next_token(p);
        if (t.type != TOKEN_IDENT) {
            parser_report(p, "Missing iteration variable name after ',' in for statement");
            return NULL;
        }
        var2 = t.sval;

        t = next_token(p);
    }

    if (t.type != TOKEN_KWORD_IN) {
        parser_report(p, "Missing 'in' keyword after iteration variable name in for statement");
        return NULL;
    }

    Node *set = parse_expr(p, 0);
    if (set == NULL)
        return NULL;

    t = next_token(p);
    if (t.type != TOKEN_COLON) {
        parser_report(p, "Missing ':' after for statement set expression");
        return NULL;
    }

    Node *body = parse_stmt(p, opflags);
    if (body == NULL)
        return NULL;

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_FOR;
    parent->left = body;
    parent->for_var1 = var1;
    parent->for_var2 = var2;
    parent->for_set  = set;

    return parent;
}

Node *parse_while_stmt(Parser *p, int opflags)
{
    Token t = next_token(p);
    if (t.type != TOKEN_KWORD_WHILE) {
        parser_report(p, "Missing keyword 'while' at the start of a while statement");
        return NULL;
    }

    Node *cond = parse_expr(p, 0);
    if (cond == NULL)
        return NULL;

    t = next_token(p);
    if (t.type != TOKEN_COLON) {
        parser_report(p, "Missing token ':' after while statement condition");
        return NULL;
    }

    Node *stmt = parse_stmt(p, opflags);
    if (stmt == NULL)
        return NULL;

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_WHILE;
    parent->left = stmt;
    parent->cond = cond;

    return parent;
}

Node *parse_block_stmt(Parser *p, bool curly)
{
    if (curly) {
        Token t = next_token(p);
        if (t.type != TOKEN_CURLY_OPEN) {
            parser_report(p, "Missing '{' at the start of a block statement");
            return NULL;
        }
    }

    Node *head;
    Node **tail = &head;

    for (;;) {

        Scanner saved = p->s;
        Token t = next_token(p);
        if (curly) {
            if (t.type == TOKEN_CURLY_CLOSE)
                break;
        } else {
            if (t.type == TOKEN_END)
                break;
        }
        p->s = saved;

        Node *node = parse_stmt(p, 0);
        if (node == NULL)
            return NULL;

        *tail = node;
        tail = &node->next;
    }

    *tail = NULL;

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_BLOCK;
    parent->left = head;

    return parent;
}

Node *parse_func_decl(Parser *p, int opflags)
{
    Token t = next_token(p);
    if (t.type != TOKEN_KWORD_FUN) {
        parser_report(p, "Missing keyword 'fun' at the start of a function declaration");
        return NULL;
    }

    t = next_token(p);
    if (t.type != TOKEN_IDENT) {
        parser_report(p, "Missing function name after 'fun' keyword");
        return NULL;
    }
    String name = t.sval;

    t = next_token(p);
    if (t.type != TOKEN_PAREN_OPEN) {
        parser_report(p, "Missing '(' after function name in declaration");
        return NULL;
    }

    Node *arg_head;
    Node **arg_tail = &arg_head;

    Scanner saved = p->s;
    t = next_token(p);
    if (t.type != TOKEN_PAREN_CLOSE) {
        p->s = saved;

        for (;;) {

            t = next_token(p);
            if (t.type != TOKEN_IDENT) {
                parser_report(p, "Missing argument name in function declaration");
                return NULL;
            }
            String argname = t.sval;

            Node *node = alloc_node(p);
            if (node == NULL)
                return NULL;

            node->type = NODE_FUNC_ARG;
            node->sval = argname;

            *arg_tail = node;
            arg_tail = &node->next;

            Scanner saved = p->s;
            t = next_token(p);
            if (t.type == TOKEN_COMMA) {
                saved = p->s;
                t = next_token(p);
            }

            if (t.type == TOKEN_PAREN_CLOSE)
                break;
            p->s = saved;
        }
    }

    *arg_tail = NULL;

    Node *body = parse_stmt(p, opflags);
    if (body == NULL)
        return NULL;

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_FUNC_DECL;
    parent->func_name = name;
    parent->func_args = arg_head;
    parent->func_body = body;

    return parent;
}

Node *parse_var_decl(Parser *p, int opflags)
{
    Token t = next_token(p);
    if (t.type != TOKEN_KWORD_LET) {
        parser_report(p, "Missing keyword 'let' at the start of a variable declaration");
        return NULL;
    }

    t = next_token(p);
    if (t.type != TOKEN_IDENT) {
        parser_report(p, "Missing variable name after 'let' keyword");
        return NULL;
    }
    String name = t.sval;

    Scanner saved = p->s;
    t = next_token(p);

    Node *value;
    if (t.type == TOKEN_OPER_ASS) {

        value = parse_expr(p, opflags);
        if (value == NULL)
            return NULL;

    } else {
        p->s = saved;
        value = NULL;
    }

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_VAR_DECL;
    parent->var_name = name;
    parent->var_value = value;

    return parent;
}

Node *parse_print_stmt(Parser *p, int opflags)
{
    Token t = next_token(p);
    if (t.type != TOKEN_KWORD_PRINT) {
        parser_report(p, "Missing keyword 'print' at the start of a print statement");
        return NULL;
    }

    Node *arg = parse_expr(p, opflags);
    if (arg == NULL)
        return NULL;

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_PRINT;
    parent->left = arg;

    return parent;
}

Node *parse_include_stmt(Parser *p)
{
    Token t = next_token(p);
    if (t.type != TOKEN_KWORD_INCLUDE) {
        parser_report(p, "Missing keyword 'include' at the start of an include statement");
        return NULL;
    }

    t = next_token(p);
    if (t.type != TOKEN_VALUE_STR) {
        parser_report(p, "Missing file path string after 'include' keyword");
        return NULL;
    }
    String path = t.sval;

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_INCLUDE;
    parent->include_path = path;
    parent->include_root = NULL;

    *p->include_tail = parent;
    p->include_tail = &parent->include_next;

    return parent;
}

Node *parse_stmt(Parser *p, int opflags)
{
    Scanner saved = p->s;
    Token t = next_token(p);
    p->s = saved;

    switch (t.type) {

        case TOKEN_KWORD_INCLUDE:
        return parse_include_stmt(p);

        case TOKEN_KWORD_PRINT:
        return parse_print_stmt(p, opflags);

        case TOKEN_KWORD_FUN:
        return parse_func_decl(p, opflags);

        case TOKEN_KWORD_LET:
        return parse_var_decl(p, opflags);

        case TOKEN_KWORD_IF:
        return parse_ifelse_stmt(p, opflags);

        case TOKEN_KWORD_WHILE:
        return parse_while_stmt(p, opflags);

        case TOKEN_KWORD_FOR:
        return parse_for_stmt(p, opflags);

        case TOKEN_CURLY_OPEN:
        return parse_block_stmt(p, true);

        default:
        break;
    }

    return parse_expr_stmt(p, opflags);
}

void print_node(Node *node)
{
    switch (node->type) {

        case NODE_VALUE_NONE:
        printf("none");
        break;

        case NODE_VALUE_TRUE:
        printf("true");
        break;

        case NODE_VALUE_FALSE:
        printf("false");
        break;

        case NODE_NESTED:
        {
            printf("(");
            print_node(node->left);
            printf(")");
        }
        break;

        case NODE_PRINT:
        {
            printf("print ");
            print_node(node->left);
        }
        break;

        case NODE_BLOCK:
        {
            printf("{");
            Node *cur = node->left;
            while (cur) {
                print_node(cur);
                printf(";");
                cur = cur->next;
            }
            printf("}");
        }
        break;

        case NODE_OPER_POS:
        printf("(");
        printf("+");
        print_node(node->left);
        printf(")");
        break;

        case NODE_OPER_NEG:
        printf("(");
        printf("-");
        print_node(node->left);
        printf(")");
        break;

        case NODE_OPER_ASS:
        printf("(");
        print_node(node->left);
        printf("=");
        print_node(node->right);
        printf(")");
        break;

        case NODE_OPER_EQL:
        printf("(");
        print_node(node->left);
        printf("==");
        print_node(node->right);
        printf(")");
        break;

        case NODE_OPER_NQL:
        printf("(");
        print_node(node->left);
        printf("!=");
        print_node(node->right);
        printf(")");
        break;

        case NODE_OPER_LSS:
        printf("(");
        print_node(node->left);
        printf("<");
        print_node(node->right);
        printf(")");
        break;

        case NODE_OPER_GRT:
        printf("(");
        print_node(node->left);
        printf(">");
        print_node(node->right);
        printf(")");
        break;

        case NODE_OPER_ADD:
        printf("(");
        print_node(node->left);
        printf("+");
        print_node(node->right);
        printf(")");
        break;

        case NODE_OPER_SUB:
        printf("(");
        print_node(node->left);
        printf("-");
        print_node(node->right);
        printf(")");
        break;

        case NODE_OPER_MUL:
        printf("(");
        print_node(node->left);
        printf("*");
        print_node(node->right);
        printf(")");
        break;

        case NODE_OPER_DIV:
        printf("(");
        print_node(node->left);
        printf("/");
        print_node(node->right);
        printf(")");
        break;

        case NODE_OPER_MOD:
        printf("(");
        print_node(node->left);
        printf("%%");
        print_node(node->right);
        printf(")");
        break;

        case NODE_VALUE_INT:
        printf("%" LLU, node->ival);
        break;

        case NODE_VALUE_FLOAT:
        printf("%f", node->dval);
        break;

        case NODE_VALUE_STR:
        printf("\"%.*s\"", node->sval.len, node->sval.ptr);
        break;

        case NODE_VALUE_VAR:
        printf("%.*s", node->sval.len, node->sval.ptr);
        break;

        case NODE_VALUE_SYSVAR:
        printf("$%.*s", node->sval.len, node->sval.ptr);
        break;

        case NODE_IFELSE:
        printf("if ");
        print_node(node->cond);
        printf(":");
        print_node(node->left);
        if (node->right) {
            printf(" else ");
            print_node(node->right);
        }
        break;

        case NODE_WHILE:
        printf("while ");
        print_node(node->cond);
        printf(":");
        print_node(node->left);
        break;

        case NODE_VALUE_HTML:
        {
            printf("<%.*s",
                node->tagname.len,
                node->tagname.ptr
            );

            Node *param = node->params;
            while (param) {
                if (param->attr_value) {
                    printf(" %.*s=",
                        param->attr_name.len,
                        param->attr_name.ptr);
                    print_node(param->attr_value);
                } else {
                    printf(" %.*s",
                        param->attr_name.len,
                        param->attr_name.ptr
                    );
                }
                param = param->next;
            }
            printf(">");

            Node *child = node->child;
            while (child) {
                print_node(child);
                child = child->next;
            }

            printf("</%.*s>",
                node->tagname.len,
                node->tagname.ptr
            );
        }
        break;

        case NODE_FOR:
        {
            printf("for %.*s",
                node->for_var1.len,
                node->for_var1.ptr
            );
            if (node->for_var2.len > 0) {
                printf(", %.*s",
                    node->for_var2.len,
                    node->for_var2.ptr
                );
            }
            printf(" in ");
            print_node(node->for_set);
            printf(": ");
            print_node(node->left);
        }
        break;

        case NODE_SELECT:
        {
            print_node(node->left);
            printf("[");
            print_node(node->right);
            printf("]");
        }
        break;

        case NODE_VALUE_ARRAY:
        {
            printf("[");
            Node *child = node->child;
            while (child) {
                print_node(child);
                printf(", ");
                child = child->next;
            }
            printf("]");
        }
        break;

        case NODE_VALUE_MAP:
        {
            printf("{");
            Node *child = node->child;
            while (child) {
                print_node(child->key);
                printf(": ");
                print_node(child);
                printf(", ");
                child = child->next;
            }
            printf("}");
        }
        break;

        case NODE_HTML_PARAM:
        {
            printf("???");
        }
        break;

        case NODE_FUNC_DECL:
        {
            printf("fun %.*s(",
                node->func_name.len,
                node->func_name.ptr);
            Node *arg = node->func_args;
            while (arg) {
                print_node(arg);
                arg = arg->next;
                if (arg)
                    printf(", ");
            }
            printf(")");
            print_node(node->func_body);
        }
        break;

        case NODE_FUNC_ARG:
        {
            printf("%.*s", node->sval.len, node->sval.ptr);
        }
        break;

        case NODE_FUNC_CALL:
        {
            print_node(node->left);
            printf("(");
            Node *arg = node->right;
            while (arg) {
                print_node(arg);
                arg = arg->next;
                if (arg)
                    printf(", ");
            }
            printf(")");
        }
        break;

        case NODE_VAR_DECL:
        {
            printf("let %.*s",
                node->var_name.len,
                node->var_name.ptr);
            if (node->var_value) {
                printf(" = ");
                print_node(node->var_value);
            }
            //printf(";");
        }
        break;

        case NODE_INCLUDE:
        {
            printf("include \"%.*s\"",
                node->include_path.len,
                node->include_path.ptr);
        }
        break;

        default:
        printf("(invalid node type %x)", node->type);
        break;
    }
}

ParseResult parse(String src, WL_Arena *a, char *errbuf, int errmax)
{
    Parser p = {
        .s={ src.ptr, src.len, 0 },
        .a=a,
        .errbuf=errbuf,
        .errmax=errmax,
        .errlen=0,
    };

    p.include_tail = &p.include_head;

    Node *node = parse_block_stmt(&p, false);
    if (node == NULL)
        return (ParseResult) { .node=NULL, .includes=NULL, .errlen=p.errlen };

    assert(node->type == NODE_BLOCK);
    node->type = NODE_GLOBAL_BLOCK;

    *p.include_tail = NULL;
    return (ParseResult) { .node=node, .includes=p.include_head, .errlen=-1 };
}
