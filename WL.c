#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef WL_NOINCLUDE
#include "wl.h"
#endif

/////////////////////////////////////////////////////////////////////////
// BASIC
/////////////////////////////////////////////////////////////////////////

typedef struct {
    char *ptr;
    int   len;
} String;

typedef struct {
    char *buf;
    int   cap;
    bool  yes;
} Error;

#define S(X) (String) { (X), SIZEOF(X)-1 }

#ifdef _WIN32
#define LLD "lld"
#define LLU "llu"
#else
#define LLD "ld"
#define LLU "lu"
#endif

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#define SIZEOF(X) (int) sizeof(X)
#define ALIGNOF(X) (int) _Alignof(X)
#define COUNT(X) (int) (sizeof(X)/sizeof((X)[0]))

#ifndef NDEBUG
#define UNREACHABLE __builtin_trap()
#define ASSERT(X) if (!(X)) __builtin_trap();
#else
#define UNREACHABLE {}
#define ASSERT(X) {}
#endif

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_printable(char c)
{
    return c >= ' ' && c <= '~';
}

static bool is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

#if 0
static char to_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    return c;
}
#endif

static int hex_digit_to_int(char c)
{
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return c - '0';
}

static bool streq(String a, String b)
{
    if (a.len != b.len)
        return false;
    for (int i = 0; i < a.len; i++)
        if (a.ptr[i] != b.ptr[i])
            return false;
    return true;
}

#if 0
static bool streqcase(String a, String b)
{
    if (a.len != b.len)
        return false;
    for (int i = 0; i < a.len; i++)
        if (to_lower(a.ptr[i]) != to_lower(b.ptr[i]))
            return false;
    return true;
}
#endif

#define REPORT(err, fmt, ...) report((err), __FILE__, __LINE__, fmt, ## __VA_ARGS__)
static void report(Error *err, char *file, int line, char *fmt, ...)
{
    if (err->yes) return;

    if (err->cap > 0) {

        va_list args;
        va_start(args, fmt);
        int len = vsnprintf(err->buf, err->cap, fmt, args);
        va_end(args);
        ASSERT(len >= 0);

        if (err->cap > len) {
            int ret = snprintf(err->buf + len, err->cap - len,
                " (reported at %s:%d)", file, line);
            ASSERT(ret >= 0);
            len += ret;
        }

        if (len > err->cap)
            len = err->cap-1;
        err->buf[len] = '\0';
    }

    err->yes = true;
}

/////////////////////////////////////////////////////////////////////////
// ARENA
/////////////////////////////////////////////////////////////////////////

static void *alloc(WL_Arena *a, int len, int align)
{
    int pad = -(intptr_t) (a->ptr + a->cur) & (align-1);
    if (a->len - a->cur < len + pad)
        return NULL;
    void *ret = a->ptr + a->cur + pad;
    a->cur += pad + len;
    return ret;
}

static bool grow_alloc(WL_Arena *a, char *p, int new_len)
{
    int new_cur = (p - a->ptr) + new_len;
    if (new_cur > a->len)
        return false;
    a->cur = new_cur;
    return true;
}

#if 0
static String copystr(String s, WL_Arena *a)
{
    char *p = alloc(a, s.len, 1);
    if (p == NULL)
        return (String) { NULL, 0 };
    memcpy(p, s.ptr, s.len);
    return (String) { p, s.len };
}
#endif

/////////////////////////////////////////////////////////////////////////
// WRITER
/////////////////////////////////////////////////////////////////////////

typedef struct {
    char *dst;
    int   cap;
    int   len;
} Writer;

static void write_raw_mem(Writer *w, void *ptr, int len)
{
    if (w->cap > w->len) {
        int cpy = MIN(w->cap - w->len, len);
        if (ptr && w->dst)
            memcpy(w->dst + w->len, ptr, cpy);
    }
    w->len += len;
}

static void write_raw_u8 (Writer *w, uint8_t  x) { write_raw_mem(w, &x, SIZEOF(x)); }
//static void write_raw_u16(Writer *w, uint16_t x) { write_raw_mem(w, &x, SIZEOF(x)); }
static void write_raw_u32(Writer *w, uint32_t x) { write_raw_mem(w, &x, SIZEOF(x)); }
//static void write_raw_u64(Writer *w, uint64_t x) { write_raw_mem(w, &x, SIZEOF(x)); }
//static void write_raw_s8 (Writer *w, int8_t   x) { write_raw_mem(w, &x, SIZEOF(x)); }
//static void write_raw_s16(Writer *w, int16_t  x) { write_raw_mem(w, &x, SIZEOF(x)); }
//static void write_raw_s32(Writer *w, int32_t  x) { write_raw_mem(w, &x, SIZEOF(x)); }
static void write_raw_s64(Writer *w, int64_t  x) { write_raw_mem(w, &x, SIZEOF(x)); }
//static void write_raw_f32(Writer *w, float    x) { write_raw_mem(w, &x, SIZEOF(x)); }
static void write_raw_f64(Writer *w, double   x) { write_raw_mem(w, &x, SIZEOF(x)); }

static void write_text(Writer *w, String str)
{
    write_raw_mem(w, str.ptr, str.len);
}

static void write_text_s64(Writer *w, int64_t n)
{
    int len;
    if (w->len < w->cap)
        len = snprintf(w->dst + w->len, w->cap - w->len, "%" LLD, n);
    else
        len = snprintf(NULL, 0, "%" LLD, n);
    ASSERT(len >= 0);
    w->len += len;
}

static void write_text_f64(Writer *w, double n)
{
    int len;
    if (w->len < w->cap)
        len = snprintf(w->dst + w->len, w->cap - w->len, "%2.2f", n);
    else
        len = snprintf(NULL, 0, "%2.2f", n);
    ASSERT(len >= 0);
    w->len += len;
}

static void patch_mem(Writer *w, void *src, int off, int len)
{
    ASSERT(off + len <= w->len);
    if (off < w->cap) {
        int cpy = MIN(w->cap - off, len);
        memcpy(w->dst + off, src, cpy);
    }
}

/////////////////////////////////////////////////////////////////////////
// PARSER
/////////////////////////////////////////////////////////////////////////

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
    TOKEN_KWORD_PROCEDURE,
    TOKEN_KWORD_LET,
    TOKEN_KWORD_NONE,
    TOKEN_KWORD_TRUE,
    TOKEN_KWORD_FALSE,
    TOKEN_KWORD_INCLUDE,
    TOKEN_KWORD_LEN,
    TOKEN_KWORD_ESCAPE,
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
    TOKEN_OPER_SHOVEL,
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
    int64_t ival;
    double  fval;
    String  sval;
} Token;

typedef enum {
    NODE_PROCEDURE_DECL,
    NODE_PROCEDURE_ARG,
    NODE_PROCEDURE_CALL,
    NODE_VAR_DECL,
    NODE_COMPOUND,
    NODE_GLOBAL,
    NODE_IFELSE,
    NODE_FOR,
    NODE_WHILE,
    NODE_INCLUDE,
    NODE_SELECT,
    NODE_NESTED,
    NODE_OPER_ESCAPE,
    NODE_OPER_LEN,
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
    NODE_OPER_SHOVEL,
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
} NodeType;

typedef struct Node Node;
struct Node {
    NodeType type;
    Node *next;

    Node *key;

    Node *left;
    Node *right;

    Node *child;

    uint64_t ival;
    double   fval;
    String   sval;

    String html_tag;
    Node*  html_attr;
    Node*  html_child;
    bool   html_body;

    Node *if_cond;
    Node *if_branch1;
    Node *if_branch2;

    Node *while_cond;
    Node *while_body;

    String for_var1;
    String for_var2;
    Node*  for_set;

    String proc_name;
    Node*  proc_args;
    Node*  proc_body;

    String var_name;
    Node*  var_value;

    String include_path;
    Node*  include_next;
    Node*  include_root;
};

typedef struct {
    Node *node;
    Node *includes;
    int   errlen;
} ParseResult;

typedef struct {
    Scanner   s;
    WL_Arena*    arena;
    char*     errbuf;
    int       errmax;
    int       errlen;
    Node*     include_head;
    Node**    include_tail;
} Parser;

static bool consume_str(Scanner *s, String x)
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

#if 0
static void write_token(Writer *w, Token token)
{
    switch (token.type) {

        default                 : write_text(w, S("???"));       break;
        case TOKEN_END          : write_text(w, S("<EOF>"));     break;
        case TOKEN_ERROR        : write_text(w, S("<ERROR>"));   break;
        case TOKEN_IDENT        : write_text(w, token.sval);     break;
        case TOKEN_KWORD_IF     : write_text(w, S("if"));        break;
        case TOKEN_KWORD_ELSE   : write_text(w, S("else"));      break;
        case TOKEN_KWORD_WHILE  : write_text(w, S("while"));     break;
        case TOKEN_KWORD_FOR    : write_text(w, S("for"));       break;
        case TOKEN_KWORD_IN     : write_text(w, S("in"));        break;
        case TOKEN_KWORD_PROCEDURE: write_text(w, S("procedure")); break;
        case TOKEN_KWORD_LET    : write_text(w, S("let"));       break;
        case TOKEN_KWORD_NONE   : write_text(w, S("none"));      break;
        case TOKEN_KWORD_TRUE   : write_text(w, S("true"));      break;
        case TOKEN_KWORD_FALSE  : write_text(w, S("false"));     break;
        case TOKEN_KWORD_INCLUDE: write_text(w, S("include"));   break;
        case TOKEN_KWORD_LEN    : write_text(w, S("len"));       break;
        case TOKEN_KWORD_ESCAPE: write_text(w, S("escape")); break;
        case TOKEN_VALUE_FLOAT  : write_text_f64(w, token.fval); break;
        case TOKEN_VALUE_INT    : write_text_s64(w, token.ival); break;
        case TOKEN_OPER_ASS     : write_text(w, S("="));         break;
        case TOKEN_OPER_EQL     : write_text(w, S("=="));        break;
        case TOKEN_OPER_NQL     : write_text(w, S("!="));        break;
        case TOKEN_OPER_LSS     : write_text(w, S("<"));         break;
        case TOKEN_OPER_GRT     : write_text(w, S(">"));         break;
        case TOKEN_OPER_ADD     : write_text(w, S("+"));         break;
        case TOKEN_OPER_SUB     : write_text(w, S("-"));         break;
        case TOKEN_OPER_MUL     : write_text(w, S("*"));         break;
        case TOKEN_OPER_DIV     : write_text(w, S("/"));         break;
        case TOKEN_OPER_MOD     : write_text(w, S("%"));         break;
        case TOKEN_OPER_SHOVEL  : write_text(w, S("<<"));        break;
        case TOKEN_PAREN_OPEN   : write_text(w, S("("));         break;
        case TOKEN_PAREN_CLOSE  : write_text(w, S(")"));         break;
        case TOKEN_BRACKET_OPEN : write_text(w, S("["));         break;
        case TOKEN_BRACKET_CLOSE: write_text(w, S("]"));         break;
        case TOKEN_CURLY_OPEN   : write_text(w, S("{"));         break;
        case TOKEN_CURLY_CLOSE  : write_text(w, S("}"));         break;
        case TOKEN_DOT          : write_text(w, S("."));         break;
        case TOKEN_COMMA        : write_text(w, S(","));         break;
        case TOKEN_COLON        : write_text(w, S(":"));         break;
        case TOKEN_DOLLAR       : write_text(w, S("$"));         break;
        case TOKEN_NEWLINE      : write_text(w, S("\\n"));       break;

        case TOKEN_VALUE_STR:
        write_text(w, S("\""));
        write_text(w, token.sval); // TODO: Escape
        write_text(w, S("\""));
        break;

    }
}
#endif

static void parser_report(Parser *p, char *fmt, ...)
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
    ASSERT(len >= 0);

    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(p->errbuf + len, p->errmax - len, fmt, args);
    va_end(args);
    ASSERT(ret >= 0);
    len += ret;

    p->errlen = len;
}

static Node *alloc_node(Parser *p)
{
    Node *n = alloc(p->arena, sizeof(Node), _Alignof(Node));
    if (n == NULL) {
        parser_report(p, "Out of memory");
        return NULL;
    }

    return n;
}

static Token next_token(Parser *p)
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

        if (streq(kword, S("if")))      return (Token) { .type=TOKEN_KWORD_IF      };
        if (streq(kword, S("else")))    return (Token) { .type=TOKEN_KWORD_ELSE    };
        if (streq(kword, S("while")))   return (Token) { .type=TOKEN_KWORD_WHILE   };
        if (streq(kword, S("for")))     return (Token) { .type=TOKEN_KWORD_FOR     };
        if (streq(kword, S("in")))      return (Token) { .type=TOKEN_KWORD_IN      };
        if (streq(kword, S("procedure"))) return (Token) { .type=TOKEN_KWORD_PROCEDURE };
        if (streq(kword, S("let")))     return (Token) { .type=TOKEN_KWORD_LET     };
        if (streq(kword, S("none")))    return (Token) { .type=TOKEN_KWORD_NONE    };
        if (streq(kword, S("true")))    return (Token) { .type=TOKEN_KWORD_TRUE    };
        if (streq(kword, S("false")))   return (Token) { .type=TOKEN_KWORD_FALSE   };
        if (streq(kword, S("include"))) return (Token) { .type=TOKEN_KWORD_INCLUDE };
        if (streq(kword, S("len")))     return (Token) { .type=TOKEN_KWORD_LEN     };
        if (streq(kword, S("escape"))) return (Token) { .type=TOKEN_KWORD_ESCAPE };

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

            return (Token) { .type=TOKEN_VALUE_FLOAT, .fval=buf };

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

            return (Token) { .type=TOKEN_VALUE_INT, .ival=buf };
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
                buf = alloc(p->arena, substr_len+1, 1);
            else
                if (!grow_alloc(p->arena, buf, len + substr_len+1))
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

    if (consume_str(&p->s, S("<<"))) return (Token) { .type=TOKEN_OPER_SHOVEL };
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

static Token next_token_or_newline(Parser *p)
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

static Node *parse_stmt(Parser *p, int opflags);
static Node *parse_expr(Parser *p, int opflags);

static Node *parse_html(Parser *p)
{
    // NOTE: The first < was already consumed

    Token t = next_token(p);
    if (t.type != TOKEN_IDENT) {
        parser_report(p, "HTML tag doesn't start with a name");
        return NULL;
    }
    String tagname = t.sval;

    Node *attr_head;
    Node **attr_tail = &attr_head;

    bool no_body = false;
    Scanner *s = &p->s;
    for (;;) {

        int off = s->cur;

        bool quotes = false;
        while (s->cur < s->len && s->src[s->cur] != '\\' && (quotes || (s->src[s->cur] != '/' && s->src[s->cur] != '>'))) {
            if (s->src[s->cur] == '"')
                quotes = !quotes;
            s->cur++;
        }

        if (s->cur > off) {

            Node *child = alloc_node(p);
            if (child == NULL)
                return NULL;

            child->type = NODE_VALUE_STR;
            child->sval = (String) { p->s.src + off, p->s.cur - off };

            *attr_tail = child;
            attr_tail = &child->next;
        }

        if (s->cur == s->len) {
            ASSERT(0); // TODO
        }
        s->cur++;

        if (s->src[s->cur-1] == '>')
            break;

        if (s->src[s->cur-1] == '/') {
            while (s->cur < s->len && is_space(s->src[s->cur]))
                s->cur++;
            if (s->cur == s->len || s->src[s->cur] != '>') {
                ASSERT(0); // TODO
            }
            s->cur++;
            no_body = true;
            break;
        }

        ASSERT(s->src[s->cur-1] == '\\');

        Node *child = parse_stmt(p, IGNORE_GRT | IGNORE_DIV);
        if (child == NULL)
            return NULL;

        *attr_tail = child;
        attr_tail = &child->next;
    }

    *attr_tail = NULL;

    Node *child_head;
    Node **child_tail = &child_head;

    if (no_body == false)
        for (;;) {

            int off = s->cur;

            while (s->cur < s->len && s->src[s->cur] != '\\' && s->src[s->cur] != '<')
                s->cur++;

            if (s->cur > off) {

                Node *child = alloc_node(p);
                if (child == NULL)
                    return NULL;

                child->type = NODE_VALUE_STR;
                child->sval = (String) { p->s.src + off, p->s.cur - off };

                *child_tail = child;
                child_tail = &child->next;
            }

            if (s->cur == s->len) {
                ASSERT(0); // TODO
            }
            s->cur++;

            if (s->src[s->cur-1] == '<') {

                Scanner saved = *s;
                t = next_token(p);
                if (t.type == TOKEN_OPER_DIV) {

                    t = next_token(p);
                    if (t.type != TOKEN_IDENT) {
                        ASSERT(0); // TODO
                    }
                    String closing_tagname = t.sval;

                    if (!streq(closing_tagname, tagname)) {
                        ASSERT(0); // TODO
                    }

                    t = next_token(p);
                    if (t.type != TOKEN_OPER_GRT) {
                        ASSERT(0);
                    }

                    break;
                }

                *s = saved;

                Node *child = parse_html(p);
                if (child == NULL)
                    return NULL;

                *child_tail = child;
                child_tail = &child->next;

            } else {

                Node *child = parse_stmt(p, IGNORE_LSS);
                if (child == NULL)
                    return NULL;

                *child_tail = child;
                child_tail = &child->next;
            }
        }

    *child_tail = NULL;

    Node *parent = alloc_node(p);
    if (parent == NULL)
        return NULL;

    parent->type = NODE_VALUE_HTML;
    parent->html_tag   = tagname;
    parent->html_attr  = attr_head;
    parent->html_child = child_head;
    parent->html_body  = !no_body;

    return parent;
}

static Node *parse_array(Parser *p)
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

static Node *parse_map(Parser *p)
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

static int precedence(Token t, int flags)
{
    switch (t.type) {

        case TOKEN_OPER_ASS:
        return 1;

        case TOKEN_OPER_SHOVEL:
        if (flags & IGNORE_LSS)
            return -1;
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

static bool right_associative(Token t)
{
    return t.type == TOKEN_OPER_ASS;
}

static Node *parse_atom(Parser *p)
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

        case TOKEN_KWORD_LEN:
        {
            Node *child = parse_atom(p);
            if (child == NULL)
                return NULL;

            Node *parent = alloc_node(p);
            if (parent == NULL)
                return NULL;

            parent->type = NODE_OPER_LEN;
            parent->left = child;

            ret = parent;
        }
        break;

        case TOKEN_KWORD_ESCAPE:
        {
            Node *child = parse_atom(p);
            if (child == NULL)
                return NULL;

            Node *parent = alloc_node(p);
            if (parent == NULL)
                return NULL;

            parent->type = NODE_OPER_ESCAPE;
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
            node->ival = t.ival;

            ret = node;
        }
        break;

        case TOKEN_VALUE_FLOAT:
        {
            Node *node = alloc_node(p);
            if (node == NULL)
                return NULL;

            node->type = NODE_VALUE_FLOAT;
            node->fval = t.fval;

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
            parser_report(p, "Invalid token inside expression");
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
                        parser_report(p, "Expected ',' after argument in procedure call");
                        return NULL;
                    }
                }
            }

            *arg_tail = NULL;

            Node *parent = alloc_node(p);
            if (parent == NULL)
                return NULL;

            parent->type = NODE_PROCEDURE_CALL;
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

static Node *parse_expr_inner(Parser *p, Node *left, int min_prec, int flags)
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
            case TOKEN_OPER_SHOVEL: parent->type = NODE_OPER_SHOVEL; break;
            default:
            parser_report(p, "Operator not implemented");
            return NULL;
        }

        left = parent;
    }

    return left;
}

static Node *parse_expr(Parser *p, int flags)
{
    Node *left = parse_atom(p);
    if (left == NULL)
        return NULL;

    return parse_expr_inner(p, left, 0, flags);
}

static Node *parse_expr_stmt(Parser *p, int opflags)
{
    Node *e = parse_expr(p, opflags);
    if (e == NULL)
        return NULL;

    return e;
}

static Node *parse_ifelse_stmt(Parser *p, int opflags)
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
    parent->if_cond = cond;
    parent->if_branch1 = if_stmt;
    parent->if_branch2 = else_stmt;

    return parent;
}

static Node *parse_for_stmt(Parser *p, int opflags)
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

static Node *parse_while_stmt(Parser *p, int opflags)
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
    parent->while_cond = cond;
    parent->while_body = stmt;

    return parent;
}

static Node *parse_compound_stmt(Parser *p, bool global)
{
    if (!global) {
        Token t = next_token(p);
        if (t.type != TOKEN_CURLY_OPEN) {
            parser_report(p, "Missing '{' at the start of a compound statement");
            return NULL;
        }
    }

    Node *head;
    Node **tail = &head;

    for (;;) {

        Scanner saved = p->s;
        Token t = next_token(p);
        if (!global) {
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

    parent->type = global ? NODE_GLOBAL : NODE_COMPOUND;
    parent->left = head;

    return parent;
}

static Node *parse_proc_decl(Parser *p, int opflags)
{
    Token t = next_token(p);
    if (t.type != TOKEN_KWORD_PROCEDURE) {
        parser_report(p, "Missing keyword 'procedure' at the start of a procedure declaration");
        return NULL;
    }

    t = next_token(p);
    if (t.type != TOKEN_IDENT) {
        parser_report(p, "Missing procedure name after 'procedure' keyword");
        return NULL;
    }
    String name = t.sval;

    t = next_token(p);
    if (t.type != TOKEN_PAREN_OPEN) {
        parser_report(p, "Missing '(' after procedure name in declaration");
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
                parser_report(p, "Missing argument name in procedure declaration");
                return NULL;
            }
            String argname = t.sval;

            Node *node = alloc_node(p);
            if (node == NULL)
                return NULL;

            node->type = NODE_PROCEDURE_ARG;
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

    parent->type = NODE_PROCEDURE_DECL;
    parent->proc_name = name;
    parent->proc_args = arg_head;
    parent->proc_body = body;

    return parent;
}

static Node *parse_var_decl(Parser *p, int opflags)
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

static Node *parse_include_stmt(Parser *p)
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

static Node *parse_stmt(Parser *p, int opflags)
{
    Scanner saved = p->s;
    Token t = next_token(p);
    p->s = saved;

    switch (t.type) {

        case TOKEN_KWORD_INCLUDE:
        return parse_include_stmt(p);

        case TOKEN_KWORD_PROCEDURE:
        return parse_proc_decl(p, opflags);

        case TOKEN_KWORD_LET:
        return parse_var_decl(p, opflags);

        case TOKEN_KWORD_IF:
        return parse_ifelse_stmt(p, opflags);

        case TOKEN_KWORD_WHILE:
        return parse_while_stmt(p, opflags);

        case TOKEN_KWORD_FOR:
        return parse_for_stmt(p, opflags);

        case TOKEN_CURLY_OPEN:
        return parse_compound_stmt(p, false);

        default:
        break;
    }

    return parse_expr_stmt(p, opflags);
}

static void write_node(Writer *w, Node *node)
{
    switch (node->type) {

        case NODE_VALUE_NONE : write_text(w, S("none")); break;
        case NODE_VALUE_TRUE : write_text(w, S("true")); break;
        case NODE_VALUE_FALSE: write_text(w, S("false")); break;

        case NODE_NESTED:
        write_text(w, S("(nested "));
        write_node(w, node->left);
        write_text(w, S(")"));
        break;

        case NODE_COMPOUND:
        {
            write_text(w, S("(compound "));
            Node *cur = node->left;
            while (cur) {
                write_node(w, cur);
                cur = cur->next;
                if (cur)
                    write_text(w, S(" "));
            }
            write_text(w, S(")"));
        }
        break;

        case NODE_GLOBAL:
        {
            write_text(w, S("(global "));
            Node *cur = node->left;
            while (cur) {
                write_node(w, cur);
                cur = cur->next;
                if (cur)
                    write_text(w, S(" "));
            }
            write_text(w, S(")"));
        }
        break;

        case NODE_OPER_LEN:
        write_text(w, S("(len "));
        write_node(w, node->left);
        write_text(w, S(")"));
        break;

        case NODE_OPER_ESCAPE:
        write_text(w, S("(escape "));
        write_node(w, node->left);
        write_text(w, S(")"));
        break;

        case NODE_OPER_POS:
        write_text(w, S("(+"));
        write_node(w, node->left);
        write_text(w, S(")"));
        break;

        case NODE_OPER_NEG:
        write_text(w, S("("));
        write_text(w, S("-"));
        write_node(w, node->left);
        write_text(w, S(")"));
        break;

        case NODE_OPER_ASS:
        write_text(w, S("("));
        write_node(w, node->left);
        write_text(w, S("="));
        write_node(w, node->right);
        write_text(w, S(")"));
        break;

        case NODE_OPER_EQL:
        write_text(w, S("("));
        write_node(w, node->left);
        write_text(w, S("=="));
        write_node(w, node->right);
        write_text(w, S(")"));
        break;

        case NODE_OPER_NQL:
        write_text(w, S("("));
        write_node(w, node->left);
        write_text(w, S("!="));
        write_node(w, node->right);
        write_text(w, S(")"));
        break;

        case NODE_OPER_LSS:
        write_text(w, S("("));
        write_node(w, node->left);
        write_text(w, S("<"));
        write_node(w, node->right);
        write_text(w, S(")"));
        break;

        case NODE_OPER_GRT:
        write_text(w, S("("));
        write_node(w, node->left);
        write_text(w, S(">"));
        write_node(w, node->right);
        write_text(w, S(")"));
        break;

        case NODE_OPER_ADD:
        write_text(w, S("("));
        write_node(w, node->left);
        write_text(w, S("+"));
        write_node(w, node->right);
        write_text(w, S(")"));
        break;

        case NODE_OPER_SUB:
        write_text(w, S("("));
        write_node(w, node->left);
        write_text(w, S("-"));
        write_node(w, node->right);
        write_text(w, S(")"));
        break;

        case NODE_OPER_MUL:
        write_text(w, S("("));
        write_node(w, node->left);
        write_text(w, S("*"));
        write_node(w, node->right);
        write_text(w, S(")"));
        break;

        case NODE_OPER_DIV:
        write_text(w, S("("));
        write_node(w, node->left);
        write_text(w, S("/"));
        write_node(w, node->right);
        write_text(w, S(")"));
        break;

        case NODE_OPER_MOD:
        write_text(w, S("("));
        write_node(w, node->left);
        write_text(w, S("%%"));
        write_node(w, node->right);
        write_text(w, S(")"));
        break;

        case NODE_OPER_SHOVEL:
        write_text(w, S("("));
        write_node(w, node->left);
        write_text(w, S("<<"));
        write_node(w, node->right);
        write_text(w, S(")"));
        break;

        case NODE_VALUE_INT:
        write_text_s64(w, node->ival);
        break;

        case NODE_VALUE_FLOAT:
        write_text_f64(w, node->fval);
        break;

        case NODE_VALUE_STR:
        write_text(w, S("\""));
        write_text(w, node->sval);
        write_text(w, S("\""));
        break;

        case NODE_VALUE_VAR:
        write_text(w, node->sval);
        break;

        case NODE_VALUE_SYSVAR:
        write_text(w, S("$"));
        write_text(w, node->sval);
        break;

        case NODE_IFELSE:
        write_text(w, S("(if "));
        write_node(w, node->if_cond);
        write_text(w, S(" "));
        write_node(w, node->if_branch1);
        if (node->if_branch2) {
            write_text(w, S(" else "));
            write_node(w, node->if_branch2);
        }
        write_text(w, S(")"));
        break;

        case NODE_WHILE:
        write_text(w, S("(while "));
        write_node(w, node->while_cond);
        write_text(w, S(" "));
        write_node(w, node->while_body);
        write_text(w, S(")"));
        break;

        case NODE_VALUE_HTML:
        {
            write_text(w, S("(html "));
            write_text(w, node->html_tag);

            Node *child = node->html_child;
            while (child) {
                write_text(w, S(" "));
                write_node(w, child);
                child = child->next;
            }

            write_text(w, S(")"));
        }
        break;

        case NODE_FOR:
        write_text(w, S("(for "));
        write_text(w, node->for_var1);
        if (node->for_var2.len > 0) {
            write_text(w, S(", "));
            write_text(w, node->for_var2);
        }
        write_text(w, S(" in "));
        write_node(w, node->for_set);
        write_text(w, S(": "));
        write_node(w, node->left);
        break;

        case NODE_SELECT:
        write_node(w, node->left);
        write_text(w, S("["));
        write_node(w, node->right);
        write_text(w, S("]"));
        break;

        case NODE_VALUE_ARRAY:
        {
            write_text(w, S("["));
            Node *child = node->child;
            while (child) {
                write_node(w, child);
                write_text(w, S(", "));
                child = child->next;
            }
            write_text(w, S("]"));
        }
        break;

        case NODE_VALUE_MAP:
        {
            write_text(w, S("{"));
            Node *child = node->child;
            while (child) {
                write_node(w, child->key);
                write_text(w, S(": "));
                write_node(w, child);
                write_text(w, S(", "));
                child = child->next;
            }
            write_text(w, S("}"));
        }
        break;

        case NODE_PROCEDURE_DECL:
        {
            write_text(w, S("(proc "));
            write_text(w, node->proc_name);
            write_text(w, S("("));
            Node *arg = node->proc_args;
            while (arg) {
                write_node(w, arg);
                arg = arg->next;
                if (arg)
                    write_text(w, S(", "));
            }
            write_text(w, S(")"));
            write_node(w, node->proc_body);
        }
        break;

        case NODE_PROCEDURE_ARG:
        write_text(w, node->sval);
        break;

        case NODE_PROCEDURE_CALL:
        {
            write_node(w, node->left);
            write_text(w, S("("));
            Node *arg = node->right;
            while (arg) {
                write_node(w, arg);
                arg = arg->next;
                if (arg)
                    write_text(w, S(", "));
            }
            write_text(w, S(")"));
        }
        break;

        case NODE_VAR_DECL:
        write_text(w, S("(let "));
        write_text(w, node->var_name);
        if (node->var_value) {
            write_text(w, S(" = "));
            write_node(w, node->var_value);
        }
        write_text(w, S(")"));
        break;

        case NODE_INCLUDE:
        write_text(w, S("include \""));
        write_text(w, node->include_path);
        write_text(w, S("\""));
        break;
    }
}

static ParseResult parse(String src, WL_Arena *arena, char *errbuf, int errmax)
{
    Parser p = {
        .s={ src.ptr, src.len, 0 },
        .arena=arena,
        .errbuf=errbuf,
        .errmax=errmax,
        .errlen=0,
    };

    p.include_tail = &p.include_head;

    Node *node = parse_compound_stmt(&p, true);
    if (node == NULL)
        return (ParseResult) { .node=NULL, .includes=NULL, .errlen=p.errlen };

    *p.include_tail = NULL;
    return (ParseResult) { .node=node, .includes=p.include_head, .errlen=-1 };
}

/////////////////////////////////////////////////////////////////////////
// CODEGEN
/////////////////////////////////////////////////////////////////////////

enum {
    OPCODE_NOPE,
    OPCODE_JUMP,
    OPCODE_JIFP,
    OPCODE_OUTPUT,
    OPCODE_SYSVAR,
    OPCODE_SYSCALL,
    OPCODE_CALL,
    OPCODE_RET,
    OPCODE_GROUP,
    OPCODE_ESCAPE,
    OPCODE_PACK,
    OPCODE_GPOP,
    OPCODE_FOR,
    OPCODE_EXIT,
    OPCODE_VARS,
    OPCODE_POP,
    OPCODE_SETV,
    OPCODE_PUSHV,
    OPCODE_PUSHI,
    OPCODE_PUSHF,
    OPCODE_PUSHS,
    OPCODE_PUSHA,
    OPCODE_PUSHM,
    OPCODE_PUSHN,
    OPCODE_PUSHT,
    OPCODE_PUSHFL,
    OPCODE_LEN,
    OPCODE_NEG,
    OPCODE_EQL,
    OPCODE_NQL,
    OPCODE_LSS,
    OPCODE_GRT,
    OPCODE_ADD,
    OPCODE_SUB,
    OPCODE_MUL,
    OPCODE_DIV,
    OPCODE_MOD,
    OPCODE_APPEND,
    OPCODE_INSERT1,
    OPCODE_INSERT2,
    OPCODE_SELECT,
};

typedef struct UnpatchedCall UnpatchedCall;
struct UnpatchedCall {
    UnpatchedCall *next;
    String         name;
    int            off;
};

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_PROCEDURE,
} SymbolType;

typedef struct {
    SymbolType type;
    String     name;
    bool       cnst;
    int        off;
} Symbol;

typedef enum {
    SCOPE_IF,
    SCOPE_ELSE,
    SCOPE_FOR,
    SCOPE_WHILE,
    SCOPE_PROC,
    SCOPE_COMPOUND,
    SCOPE_GLOBAL,
    SCOPE_ASSIGNMENT,
} ScopeType;

typedef struct {
    ScopeType type;
    int idx_syms;
    int max_vars;
    UnpatchedCall *calls;
} Scope;

#define MAX_SYMBOLS 1024
#define MAX_SCOPES 128
#define MAX_UNPATCHED_CALLS 32

typedef struct {

    Writer code;
    Writer data;

    int num_scopes;
    Scope scopes[MAX_SCOPES];

    int num_syms;
    Symbol syms[MAX_SYMBOLS];

    UnpatchedCall *free_list_calls;
    UnpatchedCall calls[MAX_UNPATCHED_CALLS];

    bool  err;
    char *errmsg;
    int   errcap;

    int data_off;

} Codegen;

static void cg_report(Codegen *cg, char *fmt, ...)
{
    if (cg->err) return;

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(cg->errmsg, cg->errcap, fmt, args);
    va_end(args);

    if (len > cg->errcap)
        len = cg->errcap-1;

    cg->errmsg[len] = '\0';
    cg->err = true;
}

static int cg_write_u8(Codegen *cg, uint8_t x)
{
    if (cg->err) return -1;

    int off = cg->code.len;
    write_raw_u8(&cg->code, x);
    return off;
}

static int cg_write_u32(Codegen *cg, uint32_t x)
{
    if (cg->err) return -1;

    int off = cg->code.len;
    write_raw_u32(&cg->code, x);
    return off;
}

static int cg_write_s64(Codegen *cg, int64_t x)
{
    if (cg->err) return -1;

    int off = cg->code.len;
    write_raw_s64(&cg->code, x);
    return off;
}

static int cg_write_f64(Codegen *cg, double x)
{
    if (cg->err) return -1;

    int off = cg->code.len;
    write_raw_f64(&cg->code, x);
    return off;
}

static void cg_write_str(Codegen *cg, String x)
{
    if (cg->err) return;

    int off = cg->data.len;
    write_text(&cg->data, x);
    write_raw_u32(&cg->code, off);
    write_raw_u32(&cg->code, x.len);
}

static void cg_patch_u8(Codegen *cg, int off, uint8_t x)
{
    if (cg->err) return;

    patch_mem(&cg->code, &x, off, SIZEOF(x));
}

static void cg_patch_u32(Codegen *cg, int off, uint32_t x)
{
    if (cg->err) return;

    patch_mem(&cg->code, &x, off, SIZEOF(x));
}

static uint32_t cg_current_offset(Codegen *cg)
{
    return cg->code.len;
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

static Scope *parent_scope(Codegen *cg)
{
    ASSERT(cg->num_scopes > 0);

    int parent = cg->num_scopes-1;
    while (cg->scopes[parent].type != SCOPE_PROC && cg->scopes[parent].type != SCOPE_GLOBAL)
        parent--;

    return &cg->scopes[parent];
}

static bool inside_assignment(Codegen *cg)
{
    ASSERT(cg->num_scopes > 0);

    int parent = cg->num_scopes-1;
    while (cg->scopes[parent].type != SCOPE_PROC
        && cg->scopes[parent].type != SCOPE_GLOBAL
        && cg->scopes[parent].type != SCOPE_ASSIGNMENT)
        parent--;

    return cg->scopes[parent].type == SCOPE_ASSIGNMENT;
}

static int count_function_vars(Codegen *cg)
{
    int n = 0;
    Scope *scope = parent_scope(cg);
    for (int i = scope->idx_syms; i < cg->num_syms; i++)
        if (cg->syms[i].type == SYMBOL_VARIABLE)
            n++;
    return n;
}

static Symbol *cg_find_symbol(Codegen *cg, String name, bool local)
{
    if (cg->err) return NULL;

    if (name.len == 0) return NULL;
    ASSERT(cg->num_scopes > 0);
    Scope *scope = local ? &cg->scopes[cg->num_scopes-1] : parent_scope(cg);
    for (int i = cg->num_syms-1; i >= scope->idx_syms; i--)
        if (streq(cg->syms[i].name, name))
            return &cg->syms[i];
    return NULL;
}

static int cg_declare_variable(Codegen *cg, String name, bool cnst)
{
    if (cg->err) return -1;

    Symbol *sym = cg_find_symbol(cg, name, true);
    if (sym) {
        cg_report(cg, "Variable declared twice");
        return -1;
    }

    if (cg->num_syms == MAX_SYMBOLS) {
        cg_report(cg, "Symbol count limit reached");
        return -1;
    }

    int off = count_function_vars(cg);

    Scope *parent = parent_scope(cg);
    parent->max_vars = MAX(parent->max_vars, off+1);

    cg->syms[cg->num_syms++] = (Symbol) {
        .type = SYMBOL_VARIABLE,
        .name = name,
        .cnst = cnst,
        .off  = off,
    };
    return off;
}

static void cg_declare_procedure(Codegen *cg, String name, int off)
{
    if (cg->err) return;

    Symbol *sym = cg_find_symbol(cg, name, true);
    if (sym) {
        cg_report(cg, "Procedure declared twice");
        return;
    }

    if (cg->num_syms == MAX_SYMBOLS) {
        cg_report(cg, "Symbol count limit reached");
        return;
    }

    cg->syms[cg->num_syms++] = (Symbol) {
        .type = SYMBOL_PROCEDURE,
        .name = name,
        .cnst = true,
        .off  = off,
    };
}

static void cg_push_scope(Codegen *cg, ScopeType type)
{
    if (cg->err) return;

    if (cg->num_scopes == MAX_SCOPES) {
        cg_report(cg, "Scope limit reached");
        return;
    }

    Scope *scope = &cg->scopes[cg->num_scopes++];
    scope->type     = type;
    scope->idx_syms = cg->num_syms;
    scope->max_vars = 0;
    scope->calls    = NULL;
}

static void cg_pop_scope(Codegen *cg)
{
    if (cg->err) return;

    ASSERT(cg->num_scopes > 0);
    Scope *scope = &cg->scopes[cg->num_scopes-1];

    Scope *parent_scope = NULL;
    if (cg->num_scopes > 1)
        parent_scope = &cg->scopes[cg->num_scopes-2];

    while (scope->calls) {

        UnpatchedCall *call = scope->calls;
        scope->calls = call->next;

        ASSERT(call - cg->calls >= 0 && call - cg->calls < MAX_UNPATCHED_CALLS);

        Symbol *sym = cg_find_symbol(cg, call->name, true);

        if (sym == NULL) {
            if (parent_scope == NULL) {
                cg_report(cg, "Undefined function '%.*s'",
                    scope->calls->name.len,
                    scope->calls->name.ptr);
                    return;
                }
            call->next = parent_scope->calls;
            parent_scope->calls = call; 
            continue;
        }

        if (sym->type != SYMBOL_PROCEDURE) {
            cg_report(cg, "Symbol '%.*s' is not a procedure", call->name.len, call->name.ptr);
            return;
        }

        cg_patch_u32(cg, call->off, sym->off);

        call->next = cg->free_list_calls;
        cg->free_list_calls = call;

        // TODO: remove
        ASSERT(cg->scopes[cg->num_scopes-1].calls == NULL || (cg->scopes[cg->num_scopes-1].calls - cg->calls >= 0 && cg->scopes[cg->num_scopes-1].calls - cg->calls < MAX_UNPATCHED_CALLS));
    }

    cg->num_syms = scope->idx_syms;
    cg->num_scopes--;
}

static void cg_append_unpatched_call(Codegen *cg, String name, int p)
{
    if (cg->err) return;

    if (cg->free_list_calls == NULL) {
        cg_report(cg, "Out of memory");
        return;
    }
    UnpatchedCall *call = cg->free_list_calls;
    cg->free_list_calls = call->next;

    ASSERT(call - cg->calls >= 0 && call - cg->calls < MAX_UNPATCHED_CALLS);

    call->name = name;
    call->off  = p;
    call->next = NULL;

    ASSERT(cg->num_scopes > 0);
    Scope *scope = &cg->scopes[cg->num_scopes-1];

    call->next = scope->calls;
    scope->calls = call;
}

static bool cg_global_scope(Codegen *cg)
{
    Scope *scope = parent_scope(cg);
    return scope->type == SCOPE_GLOBAL;
}

static void cg_flush_pushs(Codegen *cg)
{
    if (cg->data_off != -1) {
        if (cg->data_off < cg->data.len) {
            cg_write_u8(cg, OPCODE_PUSHS);
            cg_write_u32(cg, cg->data_off);
            cg_write_u32(cg, cg->data.len - cg->data_off);
        }
        cg->data_off = -1;
    }
}

static int cg_write_opcode(Codegen *cg, uint8_t opcode)
{
    ASSERT(opcode != OPCODE_PUSHS);
    cg_flush_pushs(cg);
    return cg_write_u8(cg, opcode);
}

static void cg_write_pushs(Codegen *cg, String str, bool dont_group)
{
    if (dont_group) {
        cg_flush_pushs(cg);
        cg_write_u8(cg, OPCODE_PUSHS);
        cg_write_str(cg, str);
    } else {
        if (cg->data_off == -1)
            cg->data_off = cg->data.len;
        write_raw_mem(&cg->data, str.ptr, str.len);
    }
}

static void walk_node(Codegen *cg, Node *node, bool inside_html);

static void walk_expr_node(Codegen *cg, Node *node, bool one)
{
    // TODO: remove
    ASSERT(cg->scopes[cg->num_scopes-1].calls == NULL || (cg->scopes[cg->num_scopes-1].calls - cg->calls >= 0 && cg->scopes[cg->num_scopes-1].calls - cg->calls < MAX_UNPATCHED_CALLS));

    switch (node->type) {

        case NODE_NESTED:
        walk_expr_node(cg, node->left, one);
        break;

        case NODE_OPER_LEN:
        walk_expr_node(cg, node->left, true);
        cg_write_opcode(cg, OPCODE_LEN);
        break;

        case NODE_OPER_ESCAPE:
        cg_write_opcode(cg, OPCODE_GROUP);
        walk_expr_node(cg, node->left, false);
        cg_write_opcode(cg, OPCODE_ESCAPE);
        break;

        case NODE_OPER_POS:
        walk_expr_node(cg, node->left, one);
        break;

        case NODE_OPER_NEG:
        walk_expr_node(cg, node->left, true);
        cg_write_opcode(cg, OPCODE_NEG);
        break;

        case NODE_OPER_ASS:
        {
            Node *dst = node->left;
            Node *src = node->right;

            if (dst->type == NODE_VALUE_VAR) {

                String name = dst->sval;
                Symbol *sym = cg_find_symbol(cg, name, false);
                if (sym == NULL) {
                    cg_report(cg, "Write to undeclared variable");
                    return;
                }
                if (sym->type == SYMBOL_PROCEDURE) {
                    cg_report(cg, "Symbol is not a variable");
                    return;
                }
                if (sym->cnst) {
                    cg_report(cg, "Variable is constant");
                    return;
                }

                cg_push_scope(cg, SCOPE_ASSIGNMENT);
                walk_expr_node(cg, src, true);
                cg_pop_scope(cg);

                cg_write_opcode(cg, OPCODE_SETV);
                cg_write_u8(cg, sym->off);

                if (!one)
                    cg_write_opcode(cg, OPCODE_POP);

            } else if (dst->type == NODE_SELECT) {

                cg_push_scope(cg, SCOPE_ASSIGNMENT);
                walk_expr_node(cg, src, true);
                cg_pop_scope(cg);

                walk_expr_node(cg, dst->left,  true);
                walk_expr_node(cg, dst->right, true);
                cg_write_opcode(cg, OPCODE_INSERT2);

                if (!one)
                    cg_write_opcode(cg, OPCODE_POP);

            } else {

                cg_report(cg, "Assignment left side can't be assigned to");
                return;
            }
        }
        break;

        case NODE_OPER_SHOVEL:
        {
            walk_expr_node(cg, node->left, true);

            cg_push_scope(cg, SCOPE_ASSIGNMENT);
            walk_expr_node(cg, node->right, true);
            cg_pop_scope(cg);

            cg_write_opcode(cg, OPCODE_APPEND);
            if (!one)
                cg_write_opcode(cg, OPCODE_POP);
        }
        break;

        case NODE_OPER_EQL:
        walk_expr_node(cg, node->left, true);
        walk_expr_node(cg, node->right, true);
        cg_write_opcode(cg, OPCODE_EQL);
        break;

        case NODE_OPER_NQL:
        walk_expr_node(cg, node->left, true);
        walk_expr_node(cg, node->right, true);
        cg_write_opcode(cg, OPCODE_NQL);
        break;

        case NODE_OPER_LSS:
        walk_expr_node(cg, node->left, true);
        walk_expr_node(cg, node->right, true);
        cg_write_opcode(cg, OPCODE_LSS);
        break;

        case NODE_OPER_GRT:
        walk_expr_node(cg, node->left, true);
        walk_expr_node(cg, node->right, true);
        cg_write_opcode(cg, OPCODE_GRT);
        break;

        case NODE_OPER_ADD:
        walk_expr_node(cg, node->left, true);
        walk_expr_node(cg, node->right, true);
        cg_write_opcode(cg, OPCODE_ADD);
        break;

        case NODE_OPER_SUB:
        walk_expr_node(cg, node->left, true);
        walk_expr_node(cg, node->right, true);
        cg_write_opcode(cg, OPCODE_SUB);
        break;

        case NODE_OPER_MUL:
        walk_expr_node(cg, node->left, true);
        walk_expr_node(cg, node->right, true);
        cg_write_opcode(cg, OPCODE_MUL);
        break;

        case NODE_OPER_DIV:
        walk_expr_node(cg, node->left, true);
        walk_expr_node(cg, node->right, true);
        cg_write_opcode(cg, OPCODE_DIV);
        break;

        case NODE_OPER_MOD:
        walk_expr_node(cg, node->left, true);
        walk_expr_node(cg, node->right, true);
        cg_write_opcode(cg, OPCODE_MOD);
        break;

        case NODE_VALUE_INT:
        cg_write_opcode(cg, OPCODE_PUSHI);
        cg_write_s64(cg, node->ival);
        break;

        case NODE_VALUE_FLOAT:
        cg_write_opcode(cg, OPCODE_PUSHF);
        cg_write_f64(cg, node->fval);
        break;

        case NODE_VALUE_STR:
        cg_write_pushs(cg, node->sval, one);
        break;

        case NODE_VALUE_NONE:
        cg_write_opcode(cg, OPCODE_PUSHN);
        break;

        case NODE_VALUE_TRUE:
        cg_write_opcode(cg, OPCODE_PUSHT);
        break;

        case NODE_VALUE_FALSE:
        cg_write_opcode(cg, OPCODE_PUSHFL);
        break;

        case NODE_VALUE_VAR:
        {
            String name = node->sval;
            Symbol *sym = cg_find_symbol(cg, name, false);
            if (sym == NULL) {
                cg_report(cg, "Access to undeclared variable '%.*s'", name.len, name.ptr);
                return;
            }
            if (sym->type == SYMBOL_PROCEDURE) {
                cg_report(cg, "Symbol is not a variable");
                return;
            }

            cg_write_opcode(cg, OPCODE_PUSHV);
            cg_write_u8(cg, sym->off);
        }
        break;

        case NODE_VALUE_SYSVAR:
        cg_write_opcode(cg, OPCODE_SYSVAR);
        cg_write_str(cg, node->sval);
        break;

        case NODE_VALUE_HTML:
        {
            if (one)
                cg_write_opcode(cg, OPCODE_GROUP);

            cg_write_pushs(cg, S("<"), false);
            cg_write_pushs(cg, node->html_tag, false);

            Node *child = node->html_attr;
            while (child) {
                walk_node(cg, child, true);
                child = child->next;
            }

            if (!node->html_body) {
                cg_write_pushs(cg, S("/>"), false);
            } else {
                cg_write_pushs(cg, S(">"), false);
                Node *child = node->html_child;
                while (child) {
                    walk_node(cg, child, true);
                    child = child->next;
                }
                cg_write_pushs(cg, S("</"), false);
                cg_write_pushs(cg, node->html_tag, false);
                cg_write_pushs(cg, S(">"), false);
            }

            if (one)
                cg_write_opcode(cg, OPCODE_PACK);
        }
        break;

        case NODE_VALUE_ARRAY:
        {
            cg_write_opcode(cg, OPCODE_PUSHA);
            cg_write_u32(cg, count_nodes(node->child));

            Node *child = node->child;
            while (child) {
                walk_expr_node(cg, child, true);
                cg_write_opcode(cg, OPCODE_APPEND);
                child = child->next;
            }
        }
        break;

        case NODE_VALUE_MAP:
        {
            cg_write_opcode(cg, OPCODE_PUSHM);
            cg_write_u32(cg, count_nodes(node->child));

            Node *child = node->child;
            while (child) {
                walk_expr_node(cg, child, true);
                walk_expr_node(cg, child->key, true);
                cg_write_opcode(cg, OPCODE_INSERT1);
                child = child->next;
            }
        }
        break;

        case NODE_SELECT:
        {
            Node *set = node->left;
            Node *key = node->right;
            walk_expr_node(cg, set, true);
            walk_expr_node(cg, key, true);
            cg_write_opcode(cg, OPCODE_SELECT);
        }
        break;

        case NODE_PROCEDURE_CALL:
        {
            if (one)
                cg_write_opcode(cg, OPCODE_GROUP);

            int count = 0;
            Node *arg = node->right;
            while (arg) {
                walk_expr_node(cg, arg, true);
                count++;
                arg = arg->next;
            }

            Node *proc = node->left;
            if (proc->type == NODE_VALUE_VAR) {
                
                cg_write_opcode(cg, OPCODE_CALL);
                cg_write_u8(cg, count);
                int p = cg_write_u32(cg, 0);
                cg_append_unpatched_call(cg, proc->sval, p);

            } else {

                ASSERT(proc->type == NODE_VALUE_SYSVAR);
                cg_write_opcode(cg, OPCODE_SYSCALL);
                cg_write_u8(cg, count);
                cg_write_str(cg, proc->sval);
            }

            if (one)
                cg_write_opcode(cg, OPCODE_PACK);
        }
        break;

        default:
        UNREACHABLE;
    }
}

static void walk_node(Codegen *cg, Node *node, bool inside_html)
{
    // TODO: remove
    ASSERT(cg->scopes[cg->num_scopes-1].calls == NULL || (cg->scopes[cg->num_scopes-1].calls - cg->calls >= 0 && cg->scopes[cg->num_scopes-1].calls - cg->calls < MAX_UNPATCHED_CALLS));

    switch (node->type) {

        case NODE_GLOBAL:
        for (Node *child = node->left;
            child; child = child->next) {
            walk_node(cg, child, false);
        }
        break;

        case NODE_COMPOUND:
        cg_push_scope(cg, SCOPE_COMPOUND);
        for (Node *child = node->left;
            child; child = child->next)
            walk_node(cg, child, inside_html);
        cg_pop_scope(cg);
        break;

        case NODE_PROCEDURE_DECL:
        {
            cg_push_scope(cg, SCOPE_PROC);

            cg_write_opcode(cg, OPCODE_JUMP);
            int off0 = cg_write_u32(cg, 0);

            #define MAX_ARGS 128

            int num_args = 0;
            Node *args[MAX_ARGS];

            Node *arg = node->proc_args;
            while (arg) {
                if (num_args == MAX_ARGS) {
                    cg_report(cg, "Procedure argument limit reached");
                    return;
                }
                args[num_args++] = arg;
                arg = arg->next;
            }

            for (int i = num_args-1; i >= 0; i--)
                cg_declare_variable(cg, args[i]->sval, false);

            int off1 = cg_write_opcode(cg, OPCODE_VARS);
            int off2 = cg_write_u8(cg, 0);

            walk_node(cg, node->proc_body, false);
            cg_write_opcode(cg, OPCODE_RET);

            cg_patch_u8 (cg, off2, count_function_vars(cg));
            cg_patch_u32(cg, off0, cg_current_offset(cg));

            cg_pop_scope(cg);

            cg_declare_procedure(cg, node->proc_name, off1);
        }
        break;

        case NODE_VAR_DECL:
        {
            int off = cg_declare_variable(cg, node->var_name, false);
            if (node->var_value) {
                cg_push_scope(cg, SCOPE_ASSIGNMENT);
                walk_expr_node(cg, node->var_value, true);
                cg_pop_scope(cg);
            } else
                cg_write_opcode(cg, OPCODE_PUSHN);
            cg_write_opcode(cg, OPCODE_SETV);
            cg_write_u8(cg, off);
            cg_write_opcode(cg, OPCODE_POP);
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

            if (node->if_branch2) {

                walk_expr_node(cg, node->if_cond, true);

                cg_write_opcode(cg, OPCODE_JIFP);
                int p1 = cg_write_u32(cg, 0);

                cg_push_scope(cg, SCOPE_IF);
                walk_node(cg, node->if_branch1, inside_html);
                cg_pop_scope(cg);

                cg_write_opcode(cg, OPCODE_JUMP);
                int p2 = cg_write_u32(cg, 0);

                cg_flush_pushs(cg);
                cg_patch_u32(cg, p1, cg_current_offset(cg));

                cg_push_scope(cg, SCOPE_ELSE);
                walk_node(cg, node->if_branch2, inside_html);
                cg_pop_scope(cg);

                cg_flush_pushs(cg);
                cg_patch_u32(cg, p2, cg_current_offset(cg));

            } else {

                walk_expr_node(cg, node->if_cond, true);

                cg_write_opcode(cg, OPCODE_JIFP);
                int p1 = cg_write_u32(cg, 0);

                cg_push_scope(cg, SCOPE_IF);
                walk_node(cg, node->if_branch1, inside_html);
                cg_pop_scope(cg);

                cg_flush_pushs(cg);
                cg_patch_u32(cg, p1, cg_current_offset(cg));
            }
        }
        break;

        case NODE_FOR:
        {
            cg_push_scope(cg, SCOPE_FOR);

            int var_1 = cg_declare_variable(cg, node->for_var1, false);
            int var_2 = cg_declare_variable(cg, node->for_var2, true);
            int var_3 = cg_declare_variable(cg, (String) { NULL, 0 }, true);

            walk_expr_node(cg, node->for_set, true);
            cg_write_opcode(cg, OPCODE_SETV);
            cg_write_u8(cg, var_3);
            cg_write_opcode(cg, OPCODE_POP);

            cg_write_opcode(cg, OPCODE_PUSHI);
            cg_write_s64(cg, -1);
            cg_write_opcode(cg, OPCODE_SETV);
            cg_write_u8(cg, var_2);
            cg_write_opcode(cg, OPCODE_POP);

            int start = cg_write_opcode(cg, OPCODE_FOR);
            cg_write_u8(cg, var_3);
            cg_write_u8(cg, var_1);
            cg_write_u8(cg, var_2);
            int p = cg_write_u32(cg, 0);

            walk_node(cg, node->left, inside_html);

            cg_write_opcode(cg, OPCODE_JUMP);
            cg_write_u32(cg, start);

            cg_patch_u32(cg, p, cg_current_offset(cg));

            cg_pop_scope(cg);
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

            int start = cg_current_offset(cg);

            walk_expr_node(cg, node->while_cond, true);

            cg_write_opcode(cg, OPCODE_JIFP);
            int p = cg_write_u32(cg, 0);

            cg_push_scope(cg, SCOPE_WHILE);
            walk_node(cg, node->left, inside_html);
            cg_pop_scope(cg);

            cg_write_opcode(cg, OPCODE_JUMP);
            cg_write_u32(cg, start);

            cg_patch_u32(cg, p, cg_current_offset(cg));
        }
        break;

        case NODE_INCLUDE:
        walk_node(cg, node->include_root, false);
        break;

        default:
        walk_expr_node(cg, node, false);
        if (cg_global_scope(cg) && !inside_assignment(cg) && !inside_html)
            cg_write_opcode(cg, OPCODE_OUTPUT);
        break;
    }
}

#define WL_MAGIC 0xFEEDBEEF

static int codegen(Node *node, char *dst, int cap, char *errmsg, int errcap)
{
    char *hdr;
    if (cap < SIZEOF(uint32_t) * 3)
        hdr = NULL;
    else {
        hdr = dst;
        dst += SIZEOF(uint32_t) * 3;
        cap -= SIZEOF(uint32_t) * 3;
    }

    Codegen cg = {
        .code = { dst,         cap/2, 0 },
        .data = { dst + cap/2, cap/2, 0 },
        .num_scopes = 0,
        .err = false,
        .errmsg = errmsg,
        .errcap = errcap,
        .data_off = -1,
    };

    cg.free_list_calls = cg.calls;
    for (int i = 0; i < MAX_UNPATCHED_CALLS-1; i++)
        cg.calls[i].next = &cg.calls[i+1];
    cg.calls[MAX_UNPATCHED_CALLS-1].next = NULL;

    cg_push_scope(&cg, SCOPE_GLOBAL);
    cg_write_opcode(&cg, OPCODE_VARS);
    int off = cg_write_u8(&cg, 0);
    walk_node(&cg, node, false);
    cg_write_opcode(&cg, OPCODE_EXIT);
    cg_patch_u8(&cg, off, cg.scopes[0].max_vars);
    cg_pop_scope(&cg);

    if (cg.err)
        return -1;

    if (hdr) {

        uint32_t magic = WL_MAGIC;
        uint32_t code_len = cg.code.len;
        uint32_t data_len = cg.data.len;
        memcpy(hdr + 0, &magic   , sizeof(uint32_t));
        memcpy(hdr + 4, &code_len, sizeof(uint32_t));
        memcpy(hdr + 8, &data_len, sizeof(uint32_t));

        if (cg.code.len + cg.data.len <= cap)
            memmove(dst + cg.code.len, dst + cap/2, cg.data.len);
    }

    return cg.code.len + cg.data.len + SIZEOF(uint32_t) * 3;
}

static int write_instr(Writer *w, char *src, int len, String data)
{
    if (len == 0)
        return -1;

    switch (src[0]) {

        uint8_t b0;
        uint8_t b1;
        uint8_t b2;
        uint32_t w0;
        uint32_t w1;
        int64_t i;
        double  d;

        case OPCODE_NOPE:
        write_text(w, S("NOPE\n"));
        return 1;

        case OPCODE_JUMP:
        if (len < 5) return -1;
        memcpy(&w0, src + 1, sizeof(uint32_t));
        write_text(w, S("JUMP "));
        write_text_s64(w, w0);
        write_text(w, S("\n"));
        return 5;

        case OPCODE_JIFP:
        if (len < 5) return -1;
        memcpy(&w0, src + 1, sizeof(uint32_t));
        write_text(w, S("JIFP "));
        write_text_s64(w, w0);
        write_text(w, S("\n"));
        return 5;

        case OPCODE_OUTPUT:
        write_text(w, S("OUTPUT\n"));
        return 1;

        case OPCODE_SYSVAR:
        if (len < 9) return -1;
        memcpy(&w0, src + 1, sizeof(uint32_t));
        memcpy(&w1, src + 5, sizeof(uint32_t));
        write_text(w, S("SYSVAR \""));
        write_text(w, (String) { data.ptr + w0, w1 });
        write_text(w, S("\"\n"));
        return 9;

        case OPCODE_SYSCALL:
        if (len < 10) return -1;
        memcpy(&b0, src + 1, sizeof(uint8_t));
        memcpy(&w0, src + 2, sizeof(uint32_t));
        memcpy(&w1, src + 6, sizeof(uint32_t));
        write_text(w, S("SYSCALL "));
        write_text_s64(w, b0);
        write_text(w, S(" \""));
        write_text(w, (String) { data.ptr + w0, w1 });
        write_text(w, S("\"\n"));
        return 10;

        case OPCODE_CALL:
        if (len < 6) return -1;
        memcpy(&b0, src + 1, sizeof(uint8_t));
        memcpy(&w0, src + 2, sizeof(uint32_t));
        write_text(w, S("CALL "));
        write_text_s64(w, b0);
        write_text(w, S(" "));
        write_text_s64(w, w0);
        write_text(w, S("\n"));
        return 6;

        case OPCODE_RET:
        write_text(w, S("RET\n"));
        return 1;

        case OPCODE_GROUP:
        write_text(w, S("GROUP\n"));
        return 1;

        case OPCODE_ESCAPE:
        write_text(w, S("ESCAPE\n"));
        return 1;

        case OPCODE_PACK:
        write_text(w, S("PACK\n"));
        return 1;

        case OPCODE_GPOP:
        write_text(w, S("GPOP\n"));
        return 1;

        case OPCODE_FOR:
        if (len < 8) return -1;
        memcpy(&b0, src + 1, sizeof(b0));
        memcpy(&b1, src + 2, sizeof(b1));
        memcpy(&b2, src + 3, sizeof(b2));
        memcpy(&w0, src + 4, sizeof(w0));
        write_text(w, S("FOR "));
        write_text_s64(w, b0);
        write_text(w, S(" "));
        write_text_s64(w, b1);
        write_text(w, S(" "));
        write_text_s64(w, b2);
        write_text(w, S(" "));
        write_text_s64(w, w0);
        write_text(w, S("\n"));
        return 8;

        case OPCODE_EXIT:
        write_text(w, S("EXIT\n"));
        return 1;

        case OPCODE_VARS:
        if (len < 2) return -1;
        memcpy(&b0, src + 1, sizeof(b0));
        write_text(w, S("VARS "));
        write_text_s64(w, b0);
        write_text(w, S("\n"));
        return 2;

        case OPCODE_POP:
        write_text(w, S("POP\n"));
        return 1;

        case OPCODE_SETV:
        if (len < 2) return -1;
        memcpy(&b0, src + 1, sizeof(uint8_t));
        write_text(w, S("SETV "));
        write_text_s64(w, b0);
        write_text(w, S("\n"));
        return 2;

        case OPCODE_PUSHV:
        if (len < 2) return -1;
        memcpy(&b0, src + 1, sizeof(uint8_t));
        write_text(w, S("PUSHV "));
        write_text_s64(w, b0);
        write_text(w, S("\n"));
        return 2;

        case OPCODE_PUSHI:
        if (len < 9) return -1;
        memcpy(&i, src + 1, sizeof(int64_t));
        write_text(w, S("PUSHI "));
        write_text_s64(w, i);
        write_text(w, S("\n"));
        return 9;

        case OPCODE_PUSHF:
        if (len < 9) return -1;
        memcpy(&d, src + 1, sizeof(double));
        write_text(w, S("PUSHF "));
        write_text_f64(w, d);
        write_text(w, S("\n"));
        return 9;

        case OPCODE_PUSHS:
        if (len < 9) return -1;
        memcpy(&w0, src + 1, sizeof(uint32_t));
        memcpy(&w1, src + 5, sizeof(uint32_t));
        write_text(w, S("PUSHS \""));
        write_text(w, (String) { data.ptr + w0, w1 });
        write_text(w, S("\"\n"));
        return 9;

        case OPCODE_PUSHA:
        if (len < 5) return -1;
        memcpy(&w0, src + 1, sizeof(w0));
        write_text(w, S("PUSHA "));
        write_text_s64(w, w0);
        write_text(w, S("\n"));
        return 5;

        case OPCODE_PUSHM:
        if (len < 5) return -1;
        memcpy(&w0, src + 1, sizeof(w0));
        write_text(w, S("PUSHM "));
        write_text_s64(w, w0);
        write_text(w, S("\n"));
        return 5;

        case OPCODE_PUSHN:
        write_text(w, S("PUSHN\n"));
        return 1;

        case OPCODE_PUSHT:
        write_text(w, S("PUSHT\n"));
        return 1;

        case OPCODE_PUSHFL:
        write_text(w, S("PUSHFL\n"));
        return 1;

        case OPCODE_LEN:
        write_text(w, S("LEN\n"));
        return 1;

        case OPCODE_NEG:
        write_text(w, S("NEG\n"));
        return 1;

        case OPCODE_EQL:
        write_text(w, S("EQL\n"));
        return 1;

        case OPCODE_NQL:
        write_text(w, S("NQL\n"));
        return 1;

        case OPCODE_LSS:
        write_text(w, S("LSS\n"));
        return 1;

        case OPCODE_GRT:
        write_text(w, S("GRT\n"));
        return 1;

        case OPCODE_ADD:
        write_text(w, S("ADD\n"));
        return 1;

        case OPCODE_SUB:
        write_text(w, S("SUB\n"));
        return 1;

        case OPCODE_MUL:
        write_text(w, S("MUL\n"));
        return 1;

        case OPCODE_DIV:
        write_text(w, S("DIV\n"));
        return 1;

        case OPCODE_MOD:
        write_text(w, S("MOD\n"));
        return 1;

        case OPCODE_APPEND:
        write_text(w, S("APPEND\n"));
        return 1;

        case OPCODE_INSERT1:
        write_text(w, S("INSERT1\n"));
        return 1;

        case OPCODE_INSERT2:
        write_text(w, S("INSERT2\n"));
        return 1;

        case OPCODE_SELECT:
        write_text(w, S("SELECT\n"));
        return 1;

        default:
        write_text(w, S("byte "));
        write_text_s64(w, src[0]);
        return 1;
    }

    return -1;
}

static int write_program(WL_Program program, char *dst, int cap)
{
    if (program.len < 3 * sizeof(uint32_t))
        return -1;

    uint32_t magic;
    uint32_t code_len;
    uint32_t data_len;

    memcpy(&magic   , program.ptr + 0, sizeof(uint32_t));
    memcpy(&code_len, program.ptr + 4, sizeof(uint32_t));
    memcpy(&data_len, program.ptr + 8, sizeof(uint32_t));

    if (magic != WL_MAGIC)
        return -1;

    if (code_len + data_len + 3 * sizeof(uint32_t) != program.len)
        return -1;

    String code = { program.ptr + 3 * sizeof(uint32_t)           , code_len };
    String data = { program.ptr + 3 * sizeof(uint32_t) + code_len, data_len };

    Writer w = { dst, cap, 0 };

    int cur = 0;
    while (cur < code.len) {
        write_text_s64(&w, cur);
        write_text(&w, S(": "));
        int ret = write_instr(&w, code.ptr + cur, code.len - cur, data);
        if (ret < 0) return -1;
        cur += ret;
    }

    return w.len;
}

void wl_dump_program(WL_Program program)
{
    char buf[1<<10];
    int len = write_program(program, buf, SIZEOF(buf));

    if (len < 0) {
        printf("Invalid program\n");
        return;
    }

    if (len > SIZEOF(buf)) {
        char *p = malloc(len+1);
        if (p == NULL) {
            printf("Out of memory\n");
            return;
        }
        write_program(program, p, len);
        p[len] = '\0';
        fwrite(p, 1, len, stdout);
    } else {
        fwrite(buf, 1, len, stdout);
    }
}

/////////////////////////////////////////////////////////////////////////
// COMPILER
/////////////////////////////////////////////////////////////////////////

#define FILE_LIMIT 128

typedef struct {
    String file;
    Node*  root;
    Node*  includes;
} CompiledFile;

struct WL_Compiler {

    WL_Arena*    arena;
    CompiledFile files[FILE_LIMIT];
    int          num_files;
    String       waiting_file;

    bool err;
    char msg[1<<8];
};

WL_Compiler *wl_compiler_init(WL_Arena *arena)
{
    WL_Compiler *compiler = alloc(arena, SIZEOF(WL_Compiler), _Alignof(WL_Compiler));
    if (compiler == NULL)
        return NULL;
    compiler->arena = arena;
    compiler->num_files = 0;
    compiler->waiting_file = (String) { NULL, 0 };
    compiler->err = false;
    return compiler;
}

WL_AddResult wl_compiler_add(WL_Compiler *compiler, WL_String path, WL_String content)
{
    if (compiler->err)
        return (WL_AddResult) { .type=WL_ADD_ERROR };

    ParseResult pres = parse((String) { content.ptr, content.len }, compiler->arena, compiler->msg, SIZEOF(compiler->msg));
    if (pres.node == NULL) {
        compiler->err = true;
        return (WL_AddResult) { .type=WL_ADD_ERROR };
    }

    // Make include paths relative to the parent file
    if (path.len > 0) {

        String parent = { path.ptr, path.len };

        char sep = '/';
        while (parent.len > 0 && parent.ptr[parent.len-1] != sep)
            parent.len--;

        if (parent.len > 0) {
            Node *include = pres.includes;
            while (include) {

                char *dst = alloc(compiler->arena, parent.len + include->include_path.len + 1, 1);
                if (dst == NULL) {
                    // TODO
                }

                memcpy(dst,
                    parent.ptr,
                    parent.len);
                memcpy(dst + parent.len,
                    include->include_path.ptr,
                    include->include_path.len);

                include->include_path = (String) { dst, parent.len + include->include_path.len };

                include = include->include_next;
            }
        }
    }

    CompiledFile compiled_file = {
        .file = compiler->waiting_file,
        .root = pres.node,
        .includes = pres.includes,
    };
    compiler->files[compiler->num_files++] = compiled_file;
    compiler->waiting_file = (String) { NULL, 0 };

    for (int i = 0; i < compiler->num_files; i++) {

        Node *include = compiler->files[i].includes;
        while (include) {

            ASSERT(include->type == NODE_INCLUDE);

            if (include->include_root == NULL) {
                for (int j = 0; j < compiler->num_files; j++) {
                    if (streq(include->include_path, compiler->files[j].file)) {
                        include->include_root = compiler->files[j].root;
                        break;
                    }
                }
            }

            if (include->include_root == NULL) {

                if (compiler->num_files == FILE_LIMIT) {
                    ASSERT(0); // TODO
                }

                // TODO: Make the path relative to the compiled file

                compiler->waiting_file = include->include_path;
                return (WL_AddResult) { .type=WL_ADD_AGAIN, .path={ include->include_path.ptr, include->include_path.len } };
            }

            include = include->include_next;
        }
    }

    return (WL_AddResult) { .type=WL_ADD_LINK };
}

int wl_compiler_link(WL_Compiler *compiler, WL_Program *program)
{
    if (compiler->err) return -1;

    if (compiler->num_files == 0 || compiler->waiting_file.len > 0) {
        int len = snprintf(compiler->msg, SIZEOF(compiler->msg), "Missing files in compilation unit");
        if (len > SIZEOF(compiler->msg))
            len = SIZEOF(compiler->msg)-1;
        compiler->msg[len] = '\0';
        compiler->err = true;
        return -1;
    }

    char *dst = compiler->arena->ptr + compiler->arena->cur;
    int   cap = compiler->arena->len - compiler->arena->cur;

    int len = codegen(compiler->files[0].root, dst, cap, compiler->msg, SIZEOF(compiler->msg));
    if (len < 0) {
        compiler->err = true;
        return -1;
    }
    if (len > cap) {
        int len = snprintf(compiler->msg, SIZEOF(compiler->msg), "Out of memory");
        if (len > SIZEOF(compiler->msg))
            len = SIZEOF(compiler->msg)-1;
        compiler->msg[len] = '\0';
        compiler->err = true;
        return -1;
    }

    *program = (WL_Program) { dst, len };

    compiler->arena->cur += len;
    return 0;
}

WL_String wl_compiler_error(WL_Compiler *compiler)
{
    return compiler->err
        ? (WL_String) { compiler->msg, strlen(compiler->msg) }
        : (WL_String) { NULL, 0 };
}

int wl_dump_ast(WL_Compiler *compiler, char *dst, int cap)
{
    Writer w = { dst, cap, 0 };
    for (int i = 0; i < compiler->num_files; i++) {
        write_text(&w, S("(file \""));
        write_text(&w, compiler->files[i].file);
        write_text(&w, S("\" "));
        write_node(&w, compiler->files[i].root);
        write_text(&w, S(")"));
    }
    return w.len;
}

/////////////////////////////////////////////////////////////////////////
// OBJECT MODEL
/////////////////////////////////////////////////////////////////////////

typedef enum {
    TYPE_NONE,
    TYPE_BOOL,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_ARRAY,
    TYPE_MAP,
    TYPE_ERROR,
} Type;

#define TAG_ERROR 0
#define TAG_POSITIVE_INT 1
#define TAG_NEGATIVE_INT 2
#define TAG_BOOL 3
#define TAG_NONE 4
#define TAG_PTR  5

#define VALUE_NONE  ((0 << 3) | TAG_NONE)
#define VALUE_TRUE  ((0 << 3) | TAG_BOOL)
#define VALUE_FALSE ((1 << 3) | TAG_BOOL)
#define VALUE_ERROR ((0 << 3) | TAG_ERROR)

typedef uint64_t Value;

typedef struct Extension Extension;
struct Extension {
    Extension *next;
    int count;
    int capacity;
    Value vals[];
};

typedef struct {
    Type  type;
    int   count;
    int   capacity;
    Extension *ext;
    Value vals[];
} AggregateValue;

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

static int value_convert_to_str(Value v, char *dst, int cap);

static Type value_type(Value v)
{
    switch (v & 7) {
        case TAG_ERROR: return TYPE_ERROR;
        case TAG_POSITIVE_INT: return TYPE_INT;
        case TAG_NEGATIVE_INT: return TYPE_INT;
        case TAG_BOOL : return TYPE_BOOL;
        case TAG_NONE : return TYPE_NONE;
        case TAG_PTR  : return *(Type*) (v & ~(Value) 7); break;
    }
    return TAG_ERROR;
}

static int64_t value_to_s64(Value v)
{
    ASSERT(value_type(v) == TYPE_INT);

    if ((v & 7) == TAG_POSITIVE_INT)
        return (int64_t) (v >> 3);

    if ((v & 7) == TAG_NEGATIVE_INT)
        return (int64_t) ((v >> 3) | ((Value) 7 << 61));

    IntValue *p = (IntValue*) (v & ~(Value) 7);
    return p->raw;
}

static double value_to_f64(Value v)
{
    ASSERT(value_type(v) == TYPE_FLOAT);

    FloatValue *p = (FloatValue*) (v & ~(Value) 7);
    return p->raw;
}

static String value_to_str(Value v)
{
    ASSERT(value_type(v) == TYPE_STRING);

    StringValue *p = (StringValue*) (v & ~(Value) 7);
    return (String) { p->data, p->len };
}

/*

2 bits -> 2^2 = 4

00000   0    .
00001   1    .
00010   2    .
00011   3    .
00100   4    .
00101   5    .
00110   6    .
00111   7    .
01000   8
01001   9
01010   10
01011   11
01100   12
01101   13
01110   14
01111   15
10000  -16
10001  -15
10010  -14
10011  -13
10100  -12
10101  -11
10110  -10
10111  -9
11000  -8    .
11001  -7    .
11010  -6    .
11011  -5    .
11100  -4    .
11101  -3    .
11110  -2    .
11111  -1    .

*/

static Value value_from_s64(int64_t x, WL_Arena *arena, Error *err)
{
    Value v = (Value) x;
    Value upper3bits = v >> 61;

    if (upper3bits == 0)
        return (v << 3) | TAG_POSITIVE_INT;

    if (upper3bits == 7)
        return (v << 3) | TAG_NEGATIVE_INT;

    IntValue *p = alloc(arena, SIZEOF(IntValue), _Alignof(IntValue));
    if (p == NULL) {
        REPORT(err, "Out of memory");
        return VALUE_ERROR;
    }

    p->type = TYPE_INT;
    p->raw  = x;

    ASSERT(((Value) p & 7) == 0);
    return ((Value) p) | TAG_PTR;
}

static Value value_from_f64(double x, WL_Arena *arena, Error *err)
{
    FloatValue *v = alloc(arena, SIZEOF(FloatValue), _Alignof(FloatValue));
    if (v == NULL) {
        REPORT(err, "Out of memory");
        return VALUE_ERROR;
    }

    v->type = TYPE_FLOAT;
    v->raw  = x;

    ASSERT(((uintptr_t) v & 7) == 0);
    return ((Value) v) | TAG_PTR;
}

static Value value_from_str(String x, WL_Arena *arena, Error *err)
{
    StringValue *v = alloc(arena, SIZEOF(StringValue) + x.len, 8);
    if (v == NULL) {
        REPORT(err, "Out of memory");
        return VALUE_ERROR;
    }

    v->type = TYPE_STRING;
    v->len = x.len;
    memcpy(v->data, x.ptr, x.len);

    ASSERT(((uintptr_t) v & 7) == 0);
    return ((Value) v) | TAG_PTR;
}

static Value aggregate_empty(bool map, uint32_t cap, WL_Arena *arena, Error *err)
{
    AggregateValue *v = alloc(arena, SIZEOF(AggregateValue) + 2 * cap * SIZEOF(Value), MAX(_Alignof(AggregateValue), 8));
    if (v == NULL) {
        REPORT(err, "Out of memory");
        return VALUE_ERROR;
    }

    v->type = map ? TYPE_MAP : TYPE_ARRAY;
    v->count = 0;
    v->capacity = cap;
    v->ext = NULL;

    ASSERT(((uintptr_t) v & 7) == 0);
    return ((Value) v) | TAG_PTR;
}

static int64_t aggregate_length(AggregateValue *agg)
{
    int64_t n = agg->count;

    Extension *ext = agg->ext;
    while (ext) {
        n += ext->count;
        ext = ext->next;
    }

    return n;
}

static Value *aggregate_select_by_raw_index(AggregateValue *agg, int64_t idx)
{
    ASSERT(agg->type == TYPE_ARRAY || agg->type == TYPE_MAP);

    if (idx < 0 || idx >= aggregate_length(agg))
        return NULL;

    if (idx < agg->count)
        return &agg->vals[idx];

    idx -= agg->count;
    Extension *ext = agg->ext;
    while (ext) {
        if (idx < ext->count)
            return &ext->vals[idx];
        idx -= ext->count;
        ext = ext->next;
    }

    UNREACHABLE;
    return NULL;
}

static bool value_eql(Value a, Value b);

static Value *aggregate_select(AggregateValue *agg, Value key)
{
    if (agg->type == TYPE_MAP) {

        for (int i = 0; i < agg->count; i += 2)
            if (value_eql(agg->vals[i], key))
                return &agg->vals[i+1];

        Extension *ext = agg->ext;
        while (ext) {
            for (int i = 0; i < ext->count; i += 2)
                if (value_eql(ext->vals[i], key)) {
                    return &ext->vals[i+1];
                }
            ext = ext->next;
        }

        return NULL;
    
    } else {

        ASSERT(agg->type == TYPE_ARRAY);

        if (value_type(key) != TYPE_INT)
            return NULL;
        int64_t idx = value_to_s64(key);

        return aggregate_select_by_raw_index(agg, idx);
    }
}

static bool aggregate_append(AggregateValue *agg, Value v1, Value v2, WL_Arena *arena)
{
    if (agg->count < agg->capacity) {
        agg->vals[agg->count++] = v1;
        if (v2 != VALUE_ERROR)
            agg->vals[agg->count++] = v2;
        return true;
    }

    Extension *tail = agg->ext;
    if (tail)
        while (tail->next)
            tail = tail->next;

    Extension *ext;
    if (tail == NULL || tail->count == tail->capacity) {

        int cap = 8;
        ext = alloc(arena, SIZEOF(Extension) + cap * sizeof(Value), ALIGNOF(Extension));
        if (ext == NULL)
            return false;

        ext->count = 0;
        ext->capacity = cap;
        ext->next = NULL;

        if (tail)
            tail->next = ext;
        else
            agg->ext = ext;

    } else
        ext = tail;

    ext->vals[ext->count++] = v1;
    if (v2 != VALUE_ERROR)
        ext->vals[ext->count++] = v2;
    return true;
}

static Value value_empty_map(uint32_t cap, WL_Arena *arena, Error *err)
{
    return aggregate_empty(true, 2 * cap, arena, err);
}

static Value value_empty_array(uint32_t cap, WL_Arena *arena, Error *err)
{
    return aggregate_empty(false, cap, arena, err);
}

static int64_t value_length(Value set)
{
    ASSERT(value_type(set) == TYPE_MAP || value_type(set) == TYPE_ARRAY);
    AggregateValue *agg = (void*) (set & ~(Value) 7);
    int64_t len = aggregate_length(agg);
    if (agg->type == TYPE_MAP)
        len /= 2;
    return len;
}

static bool value_insert(Value set, Value key, Value val, WL_Arena *arena, Error *err)
{
    Type t = value_type(set);
    if (t != TYPE_MAP && t != TYPE_ARRAY) {
        REPORT(err, "Invalid insertion on non-map and non-array value");
        return false;
    }
    AggregateValue *agg = (void*) (set & ~(Value) 7);

    Value *dst = aggregate_select(agg, key);
    if (dst != NULL) {
        *dst = val;
        return true;
    }

    if (agg->type == TYPE_ARRAY && value_type(key) != TYPE_INT) {
        REPORT(err, "Invalid index used in array access");
        return false;
    }

    if (!aggregate_append(agg, key, val, arena)) {
        REPORT(err, "Out of memory");
        return false;
    }

    return true;
}

static Value value_select(Value set, Value key, Error *err)
{
    Type t = value_type(set);
    if (t != TYPE_MAP && t != TYPE_ARRAY) {
        REPORT(err, "Invalid selection from non-map and non-array value");
        return VALUE_ERROR;
    }
    AggregateValue *agg = (void*) (set & ~(Value) 7);

    Value *dst = aggregate_select(agg, key);
    if (dst) return *dst;

    if (agg->type == TYPE_ARRAY && value_type(key) != TYPE_INT) {
        REPORT(err, "Invalid index used in array access");
        return VALUE_ERROR;
    }

    char key_buf[1<<8];
    int key_len = value_convert_to_str(key, key_buf, SIZEOF(key_buf));
    if (key_len > SIZEOF(key_buf)-1)
        key_len = SIZEOF(key_buf)-1;
    key_buf[key_len] = '\0';

    char set_buf[1<<8];
    int set_len = value_convert_to_str(set, set_buf, SIZEOF(set_buf));
    if (set_len > SIZEOF(set_buf)-1)
        set_len = SIZEOF(set_buf)-1;
    set_buf[set_len] = '\0';

    REPORT(err, "Invalid key '%s' used in access to map '%s'", key_buf, set_buf);
    return VALUE_ERROR;
}

static Value value_select_by_index(Value set, int64_t idx, Error *err)
{
    Type t = value_type(set);
    if (t != TYPE_MAP && t != TYPE_ARRAY) {
        REPORT(err, "Invalid selection from non-map and non-array value");
        return VALUE_ERROR;
    }
    AggregateValue *agg = (void*) (set & ~(Value) 7);

    if (agg->type == TYPE_MAP)
        idx *= 2;

    Value *src = aggregate_select_by_raw_index(agg, idx);
    if (src == NULL) {
        REPORT(err, "Invalid selection from non-map and non-array value");
        return VALUE_ERROR;
    }

    return *src;
}

static bool value_append(Value set, Value val, WL_Arena *arena, Error *err)
{
    Type t = value_type(set);
    if (t != TYPE_ARRAY) {
        REPORT(err, "Invalid append on non-array value");
        return false;
    }
    AggregateValue *agg = (void*) (set & ~(Value) 7);

    if (!aggregate_append(agg, val, VALUE_ERROR, arena)) {
        REPORT(err, "Out of memory");
        return false;
    }

    return true;
}

static bool value_eql(Value a, Value b)
{
    Type t1 = value_type(a);
    Type t2 = value_type(b);

    if (t1 != t2)
        return false;

    switch (t1) {

        case TYPE_NONE:
        return true;

        case TYPE_BOOL:
        return a == b;

        case TYPE_INT:
        return value_to_s64(a) == value_to_s64(b);

        case TYPE_FLOAT:
        return value_to_f64(a) == value_to_f64(b);

        case TYPE_MAP:
        return false; // TODO

        case TYPE_ARRAY:
        return false; // TODO

        case TYPE_STRING:
        return streq(value_to_str(a), value_to_str(b));

        case TYPE_ERROR:
        return true;
    }

    return false;
}

static bool value_nql(Value a, Value b)
{
    return !value_eql(a, b);
}

#define TYPE_PAIR(X, Y) (((uint16_t) (X) << 16) | (uint16_t) (Y))

bool value_greater(Value a, Value b, Error *err)
{
    Type t1 = value_type(a);
    Type t2 = value_type(b);
    switch (TYPE_PAIR(t1, t2)) {
        case TYPE_PAIR(TYPE_INT  , TYPE_INT  ): return value_to_s64(a) > value_to_s64(b);
        case TYPE_PAIR(TYPE_INT  , TYPE_FLOAT): return value_to_s64(a) > value_to_f64(b);
        case TYPE_PAIR(TYPE_FLOAT, TYPE_INT  ): return value_to_f64(a) > value_to_s64(b);
        case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT): return value_to_f64(a) > value_to_f64(b);
        default:break;
    }
    REPORT(err, "Invalid '>' operation on non-numeric type");
    return false;
}

bool value_lower(Value a, Value b, Error *err)
{
    Type t1 = value_type(a);
    Type t2 = value_type(b);
    switch (TYPE_PAIR(t1, t2)) {
        case TYPE_PAIR(TYPE_INT  , TYPE_INT  ): return value_to_s64(a) < value_to_s64(b);
        case TYPE_PAIR(TYPE_INT  , TYPE_FLOAT): return value_to_s64(a) < value_to_f64(b);
        case TYPE_PAIR(TYPE_FLOAT, TYPE_INT  ): return value_to_f64(a) < value_to_s64(b);
        case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT): return value_to_f64(a) < value_to_f64(b);
        default:break;
    }
    REPORT(err, "Invalid '<' operation on non-numeric type");
    return false;
}

static Value value_neg(Value v, WL_Arena *arena, Error *err)
{
    Type t = value_type(v);
    if (t == TYPE_INT)
        return value_from_s64(-value_to_s64(v), arena, err); // TODO: overflow
    
    if (t == TYPE_FLOAT)
        return value_from_f64(-value_to_f64(v), arena, err);

    REPORT(err, "Invalid '-' operation on non-numeric type");
    return VALUE_ERROR;
}

static Value value_add(Value v1, Value v2, WL_Arena *arena, Error *err)
{
    Type t1 = value_type(v1);
    Type t2 = value_type(v2);

    Value r;
    switch (TYPE_PAIR(t1, t2)) {

        case TYPE_PAIR(TYPE_INT, TYPE_INT):
        {
            int64_t u = value_to_s64(v1);
            int64_t v = value_to_s64(v2);
            // TODO: check overflow and underflow
            r = value_from_s64(u + v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
        {
            double u = (double) value_to_s64(v1);
            double v = value_to_f64(v2);
            r = value_from_f64(u + v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
        {
            double u = value_to_f64(v1);
            double v = (double) value_to_s64(v2);
            r = value_from_f64(u + v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
        {
            double u = value_to_f64(v1);
            double v = value_to_f64(v2);
            // TODO: check overflow and underflow
            r = value_from_f64(u + v, arena, err);
        }
        break;

        default:
        REPORT(err, "Invalid operation '+' on non-numeric value");
        return VALUE_ERROR;
    }

    return r;
}

static Value value_sub(Value v1, Value v2, WL_Arena *arena, Error *err)
{
    Type t1 = value_type(v1);
    Type t2 = value_type(v2);

    Value r;
    switch (TYPE_PAIR(t1, t2)) {

        case TYPE_PAIR(TYPE_INT, TYPE_INT):
        {
            int64_t u = value_to_s64(v1);
            int64_t v = value_to_s64(v2);
            // TODO: check overflow and underflow
            r = value_from_s64(u - v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
        {
            double u = (double) value_to_s64(v1);
            double v = value_to_f64(v2);
            r = value_from_f64(u - v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
        {
            double u = value_to_f64(v1);
            double v = (double) value_to_s64(v2);
            r = value_from_f64(u - v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
        {
            double u = value_to_f64(v1);
            double v = value_to_f64(v2);
            // TODO: check overflow and underflow
            r = value_from_f64(u - v, arena, err);
        }
        break;

        default:
        REPORT(err, "Invalid operation '-' on non-numeric value");
        return VALUE_ERROR;
    }

    return r;
}

static Value value_mul(Value v1, Value v2, WL_Arena *arena, Error *err)
{
    Type t1 = value_type(v1);
    Type t2 = value_type(v2);

    Value r;
    switch (TYPE_PAIR(t1, t2)) {

        case TYPE_PAIR(TYPE_INT, TYPE_INT):
        {
            int64_t u = value_to_s64(v1);
            int64_t v = value_to_s64(v2);
            // TODO: check overflow and underflow
            r = value_from_s64(u * v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
        {
            double u = (double) value_to_s64(v1);
            double v = value_to_f64(v2);
            r = value_from_f64(u * v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
        {
            double u = value_to_f64(v1);
            double v = (double) value_to_s64(v2);
            r = value_from_f64(u * v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
        {
            double u = value_to_f64(v1);
            double v = value_to_f64(v2);
            // TODO: check overflow and underflow
            r = value_from_f64(u * v, arena, err);
        }
        break;

        default:
        REPORT(err, "Invalid operation '*' on non-numeric value");
        return VALUE_ERROR;
    }

    return r;
}

static Value value_div(Value v1, Value v2, WL_Arena *arena, Error *err)
{
    Type t1 = value_type(v1);
    Type t2 = value_type(v2);

    Value r;
    switch (TYPE_PAIR(t1, t2)) {

        case TYPE_PAIR(TYPE_INT, TYPE_INT):
        {
            // TODO: check division by 0

            int64_t u = value_to_s64(v1);
            int64_t v = value_to_s64(v2);
            r = value_from_s64(u / v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_INT, TYPE_FLOAT):
        {
            // TODO: check division by 0

            double u = (double) value_to_s64(v1);
            double v = value_to_f64(v2);
            r = value_from_f64(u / v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_FLOAT, TYPE_INT):
        {
            // TODO: check division by 0

            double u = value_to_f64(v1);
            double v = (double) value_to_s64(v2);
            r = value_from_f64(u / v, arena, err);
        }
        break;

        case TYPE_PAIR(TYPE_FLOAT, TYPE_FLOAT):
        {
            double u = value_to_f64(v1);
            double v = value_to_f64(v2);
            r = value_from_f64(u / v, arena, err);
        }
        break;

        default:
        REPORT(err, "Invalid operation '/' on non-numeric value");
        return VALUE_ERROR;
    }

    return r;
}

static Value value_mod(Value v1, Value v2, WL_Arena *arena, Error *err)
{
    Type t1 = value_type(v1);
    Type t2 = value_type(v2);

    if (t1 != TYPE_INT || t2 != TYPE_INT) {
        REPORT(err, "Invalid operation '%%' on non-integer value");
        return VALUE_ERROR;
    }

    int64_t u = value_to_s64(v1);
    int64_t v = value_to_s64(v2);
    Value r = value_from_s64(u % v, arena, err);
    return r;
}

static void value_convert_to_str_inner(Writer *w, Value v)
{
    Type t = value_type(v);
    switch (t) {

        case TYPE_NONE:
        break;

        case TYPE_BOOL:
        write_text(w, v == VALUE_TRUE ? S("true") : S("false"));
        break;

        case TYPE_INT:
        write_text_s64(w, value_to_s64(v));
        break;

        case TYPE_FLOAT:
        write_text_f64(w, value_to_f64(v));
        break;

        case TYPE_STRING:
        write_text(w, value_to_str(v));
        break;

        case TYPE_ARRAY:
        {
            AggregateValue *agg = (void*) (v & ~(Value) 7);
            for (int i = 0; i < agg->count; i++)
                value_convert_to_str_inner(w, agg->vals[i]);
            Extension *ext = agg->ext;
            while (ext) {
                for (int i = 0; i < ext->count; i++)
                    value_convert_to_str_inner(w, ext->vals[i]);
                ext = ext->next;
            }
        }
        break;

        case TYPE_MAP:
        write_text(w, S("<map>"));
        break;

        case TYPE_ERROR:
        break;
    }
}

static int value_convert_to_str(Value v, char *dst, int cap)
{
    Writer w = { dst, cap, 0};
    value_convert_to_str_inner(&w, v);
    return w.len;
}

static Value value_escape_packed(Value v, WL_Arena *arena, Error *err);

static int array_escape(Value v, Value *out, int max, WL_Arena *arena, Error *err)
{
    Value v2 = value_empty_array(value_length(v), arena, err);
    if (v2 == VALUE_ERROR) return -1;

    AggregateValue *src = (void*) (v  & ~(Value) 7);

    for (int i = 0; i < src->count; i++) {

        Value child = src->vals[i];

        Value escaped_child = value_escape_packed(child, arena, err);
        if (escaped_child == VALUE_ERROR)
            return -1;

        if (!value_append(v2, escaped_child, arena, err))
            return -1;
    }
    Extension *ext = src->ext;
    while (ext) {
        for (int i = 0; i < ext->count; i++) {

            Value child = src->vals[i];

            Value escaped_child = value_escape_packed(child, arena, err);
            if (escaped_child == VALUE_ERROR)
                return -1;

            if (!value_append(v2, escaped_child, arena, err))
                return -1;
        }
        ext = ext->next;
    }

    if (max == 0)
        return -1;
    out[0] = v2;
    return 1;
}

static int string_escape(Value v, Value *out, int max, WL_Arena *arena, Error *err)
{
    String s = value_to_str(v);

    int i = 0;
    int num = 0;
    for (;;) {

        int off = i;
        while (i < s.len
            && s.ptr[i] != '<'
            && s.ptr[i] != '>'
            && s.ptr[i] != '&'
            && s.ptr[i] != '"'
            && s.ptr[i] != '\'')
            i++;
        String substr = { s.ptr + off, i - off };

        Value escaped_v = value_from_str(substr, arena, err); // TODO: don't copy the string
        if (escaped_v == VALUE_ERROR) return -1;

        if (num == max) {
            REPORT(err, "Escape buffer limit reached");
            return -1;
        }
        out[num++] = escaped_v;

        if (i == s.len) break;

        switch (s.ptr[i++]) {
            case '<' : escaped_v = value_from_str(S("&lt;"),   arena, err); break; // TODO: don't come these strings
            case '>' : escaped_v = value_from_str(S("&gt;"),   arena, err); break;
            case '&' : escaped_v = value_from_str(S("&amp;"),  arena, err); break;
            case '"' : escaped_v = value_from_str(S("&quot;"), arena, err); break;
            case '\'': escaped_v = value_from_str(S("&#x27;"), arena, err); break;
        }
        if (escaped_v == VALUE_ERROR) return -1;

        if (num == max) {
            REPORT(err, "Escape buffer limit reached");
            return -1;
        }
        out[num++] = escaped_v;
    }

    return num;
}

static int value_escape(Value v, Value *out, int max, WL_Arena *arena, Error *err)
{
    Type t = value_type(v);

    if (t == TYPE_ARRAY)
        return array_escape(v, out, max, arena, err);

    if (t == TYPE_STRING)
        return string_escape(v, out, max, arena, err);

    if (max < 1)
        return -1;
    out[0] = v;
    return 1;
}

static Value value_escape_packed(Value v, WL_Arena *arena, Error *err)
{
    Value tmp[32];
    int num = value_escape(v, tmp, COUNT(tmp), arena, err);
    if (num < 0) return VALUE_ERROR;

    Value escaped_v;

    if (num > 1) {

        Value packed = value_empty_array(num, arena, err);
        if (packed == VALUE_ERROR)
            return VALUE_ERROR;

        for (int j = 0; j < num; j++)
            if (!value_append(packed, tmp[j], arena, err))
                return VALUE_ERROR;
        escaped_v = packed;

    } else {

        ASSERT(num == 1);
        escaped_v = tmp[0];
    }

    return escaped_v;
}

#undef TYPE_PAIR

/////////////////////////////////////////////////////////////////////////
// RUNTIME
/////////////////////////////////////////////////////////////////////////

#define MAX_STACK 1024
#define MAX_FRAMES 1024
#define MAX_GROUPS 8

typedef struct {
    int retaddr;
    int varbase;
} Frame;

typedef enum {
    RUNTIME_BEGIN,
    RUNTIME_LOOP,
    RUNTIME_DONE,
    RUNTIME_ERROR,
    RUNTIME_OUTPUT,
    RUNTIME_SYSVAR,
    RUNTIME_SYSCALL,
} RuntimeState;

struct WL_Runtime {

    RuntimeState state;

    String code;
    String data;
    int off;

    int vars;
    int stack;
    Value values[MAX_STACK];

    int num_frames;
    Frame frames[MAX_FRAMES];

    int num_groups;
    int groups[MAX_GROUPS];

    WL_Arena *arena;

    char  msg[128];
    Error err;

    int stack_before_user;
    String str_for_user;
    int num_output;
    int cur_output;
    char buf[128];
};

WL_Runtime *wl_runtime_init(WL_Arena *arena, WL_Program program)
{
    if (program.len < 3 * sizeof(uint32_t))
        return NULL;

    uint32_t magic;
    uint32_t code_len;
    uint32_t data_len;

    memcpy(&magic   , program.ptr + 0, sizeof(uint32_t));
    memcpy(&code_len, program.ptr + 4, sizeof(uint32_t));
    memcpy(&data_len, program.ptr + 8, sizeof(uint32_t));

    if (magic != WL_MAGIC)
        return NULL;

    String code = { program.ptr + sizeof(uint32_t) * 3           , code_len };
    String data = { program.ptr + sizeof(uint32_t) * 3 + code_len, data_len };

    WL_Runtime *rt = alloc(arena, SIZEOF(WL_Runtime), ALIGNOF(WL_Runtime));
    if (rt == NULL)
        return NULL;

    *rt = (WL_Runtime) {
        .state      = RUNTIME_BEGIN,
        .code       = code,
        .data       = data,
        .off        = 0,
        .stack      = 0,
        .vars       = MAX_STACK-1,
        .num_frames = 0,
        .arena      = arena,
        .err        = { NULL, 0, false },
    };
    rt->err.buf = rt->msg;
    rt->err.cap = SIZEOF(rt->msg);

    rt->frames[rt->num_frames++] = (Frame) {
        .retaddr = 0,
        .varbase = rt->vars,
    };

    return rt;
}

WL_String wl_runtime_error(WL_Runtime *rt)
{
    return rt->err.yes
        ? (WL_String) { rt->msg, strlen(rt->msg) }
        : (WL_String) { NULL, 0 };
}

static void rt_read_mem(WL_Runtime *r, void *dst, int len)
{
    ASSERT(r->off + len <= r->code.len);
    memcpy(dst, r->code.ptr + r->off, len);
    r->off += len;
}

static uint8_t rt_read_u8(WL_Runtime *rt)
{
    ASSERT(rt->state == RUNTIME_LOOP);

    uint8_t x;
    rt_read_mem(rt, &x, SIZEOF(x));

    return x;
}

static uint32_t rt_read_u32(WL_Runtime *rt)
{
    ASSERT(rt->state == RUNTIME_LOOP);

    uint32_t x;
    rt_read_mem(rt, &x, SIZEOF(x));

    return x;
}

static int64_t rt_read_s64(WL_Runtime *rt)
{
    ASSERT(rt->state == RUNTIME_LOOP);

    int64_t x;
    rt_read_mem(rt, &x, SIZEOF(x));

    return x;
}

static double rt_read_f64(WL_Runtime *rt)
{
    ASSERT(rt->state == RUNTIME_LOOP);

    double x;
    rt_read_mem(rt, &x, SIZEOF(x));

    return x;
}

static String rt_read_str(WL_Runtime *rt)
{
    ASSERT(rt->state == RUNTIME_LOOP);
    uint32_t off = rt_read_u32(rt);
    uint32_t len = rt_read_u32(rt);
    ASSERT(off + len <= (uint32_t) rt->data.len);
    return (String) { rt->data.ptr + off, len };
}

static Value *rt_variable(WL_Runtime *rt, uint8_t x)
{
    ASSERT(rt->num_frames > 0);

    Frame *frame = &rt->frames[rt->num_frames-1];

    ASSERT(frame->varbase - x >= 0
        && frame->varbase - x < MAX_STACK);

    return &rt->values[frame->varbase - x];
}

static int values_usage(WL_Runtime *rt)
{
    int num_vars = (MAX_STACK - rt->vars - 1);
    return rt->stack + num_vars;
}

static bool rt_check_stack(WL_Runtime *rt, int min)
{
    if (MAX_STACK - values_usage(rt) < min) {
        REPORT(&rt->err, "Out of stack");
        rt->state = RUNTIME_ERROR;
        return false;
    }
    return true;
}

static bool rt_push_frame(WL_Runtime *rt, uint8_t args)
{
    if (rt->num_frames == MAX_FRAMES) {
        REPORT(&rt->err, "Call stack limit reached");
        rt->state = RUNTIME_ERROR;
        return false;
    }

    if (MAX_STACK - values_usage(rt) < args) {
        REPORT(&rt->err, "Stack limit reached");
        rt->state = RUNTIME_ERROR;
        return false;
    }

    Frame *frame = &rt->frames[rt->num_frames++];
    frame->retaddr = rt->off;
    frame->varbase = rt->vars;

    for (int i = 0; i < args; i++)
        rt->values[rt->vars--] = rt->values[--rt->stack];

    return true;
}

static void rt_pop_frame(WL_Runtime *rt)
{
    ASSERT(rt->num_frames > 0);
    Frame *frame = &rt->frames[rt->num_frames-1];
    rt->off  = frame->retaddr;
    rt->vars = frame->varbase;
    rt->num_frames--;
}

static void rt_set_frame_vars(WL_Runtime *rt, uint8_t num)
{
    ASSERT(rt->num_frames > 0);
    Frame *frame = &rt->frames[rt->num_frames-1];
    int num_vars = frame->varbase - rt->vars;
    if (num_vars < num)
        for (int i = 0; i < num - num_vars; i++)
            rt->values[rt->vars - i] = VALUE_NONE;
    rt->vars = frame->varbase - num;
}

static void rt_push_group(WL_Runtime *rt)
{
    if (rt->num_groups == MAX_GROUPS) {
        REPORT(&rt->err, "Out of memory");
        rt->state = RUNTIME_ERROR;
        return;
    }
    rt->groups[rt->num_groups++] = rt->stack;
}

static void rt_pack_group(WL_Runtime *rt)
{
    if (!rt_check_stack(rt, 1))
        return;

    ASSERT(rt->num_groups > 0);
    int start = rt->groups[--rt->num_groups];
    int end = rt->stack;

    if (end - start > 1) {

        Value set = value_empty_array(end - start, rt->arena, &rt->err);
        if (set == VALUE_ERROR)
            return;

        for (int i = start; i < end; i++)
            if (!value_append(set, rt->values[i], rt->arena, &rt->err))
                return;

        rt->stack = start;
        rt->values[rt->stack++] = set;
    }
}

static void rt_pop_group(WL_Runtime *rt)
{
    ASSERT(rt->num_groups > 0);
    rt->stack = rt->groups[--rt->num_groups];
}

static void value_print(Value v)
{
    char buf[1<<8];
    int len = value_convert_to_str(v, buf, SIZEOF(buf));
    if (len < SIZEOF(buf))
        fwrite(buf, 1, len, stdout);
    else {
        len = SIZEOF(buf)-1;
        fwrite(buf, 1, len, stdout);
        fprintf(stdout, " [...]");
    }
    putc('\n', stdout);
    fflush(stdout);
}

static void step(WL_Runtime *rt)
{
    switch (rt_read_u8(rt)) {

        Type t;
        Value v1;
        Value v2;
        Value v3;
        uint32_t o;
        uint8_t  b1;
        uint8_t  b2;
        uint8_t  b3;
        int64_t  i;
        double   f;
        String   s;

        case OPCODE_NOPE:
        break;

        case OPCODE_JUMP:
        rt->off = rt_read_u32(rt);
        break;

        case OPCODE_JIFP:
        ASSERT(rt->stack > 0);
        o = rt_read_u32(rt);
        v1 = rt->values[--rt->stack];
        if (v1 == VALUE_FALSE)
            rt->off = o;
        else if (value_type(v1) != TYPE_BOOL) {
            REPORT(&rt->err, "Invalid non-boolean condition");
            rt->state = RUNTIME_ERROR;
            break;
        }
        break;

        case OPCODE_VARS:
        b1 = rt_read_u8(rt);
        rt_set_frame_vars(rt, b1);
        break;

        case OPCODE_OUTPUT:
        if (rt->stack > 0) {
            rt->cur_output = 0;
            rt->num_output = rt->stack;
            rt->state = RUNTIME_OUTPUT;
        }
        break;

        case OPCODE_SYSVAR:
        s = rt_read_str(rt);
        rt_push_frame(rt, 0);
        rt->stack_before_user = rt->stack;
        rt->str_for_user = s;
        rt->state = RUNTIME_SYSVAR;
        break;

        case OPCODE_SYSCALL:
        b1 = rt_read_u8(rt);
        s = rt_read_str(rt);
        rt_push_frame(rt, b1);
        rt->stack_before_user = rt->stack;
        rt->str_for_user = s;
        rt->state = RUNTIME_SYSCALL;
        break;

        case OPCODE_CALL:
        b1 = rt_read_u8(rt);
        o = rt_read_u32(rt);
        rt_push_frame(rt, b1);
        rt->off = o;
        break;

        case OPCODE_RET:
        rt_pop_frame(rt);
        break;

        case OPCODE_GROUP:
        rt_push_group(rt);
        break;

        case OPCODE_ESCAPE:
        {
            ASSERT(rt->num_groups > 0);
            int start = rt->groups[--rt->num_groups];
            int end = rt->stack;

            Value escaped[256];
            int num_escaped = 0;

            for (int i = start; i < end; i++) {
                Value v = rt->values[i];
                int num = value_escape(v, escaped + num_escaped, COUNT(escaped) - num_escaped, rt->arena, &rt->err);
                if (num < 0) break;
                num_escaped += num;
            }

            if (num_escaped > COUNT(escaped)) {
                REPORT(&rt->err, "Escape buffer limit reached");
                rt->state = RUNTIME_ERROR;
                break;
            }

            rt->stack = start;
            if (!rt_check_stack(rt, num_escaped)) break;

            for (int i = 0; i < num_escaped; i++)
                rt->values[rt->stack + i] = escaped[i];
            rt->stack += num_escaped;
        }
        break;

        case OPCODE_PACK:
        rt_pack_group(rt);
        break;

        case OPCODE_GPOP:
        rt_pop_group(rt);
        break;

        case OPCODE_FOR:
        b1 = rt_read_u8(rt);
        b2 = rt_read_u8(rt);
        b3 = rt_read_u8(rt);
        o  = rt_read_u32(rt);

        v1 = *rt_variable(rt, b3);
        ASSERT(value_type(v1) == TYPE_INT);
        i = value_to_s64(v1);

        v2 = *rt_variable(rt, b1);

        if (value_length(v2)-1 == i) {
            rt->off = o;
            break;
        }
        i++;

        v1 = value_select_by_index(v2, i, &rt->err);
        if (v1 == VALUE_ERROR) break;

        *rt_variable(rt, b2) = v1;

        v1 = value_from_s64(i, rt->arena, &rt->err); // TODO: this could be in-place
        *rt_variable(rt, b3) = v1;
        break;

        case OPCODE_EXIT:
        rt->state = RUNTIME_DONE;
        break;

        case OPCODE_POP:
        ASSERT(rt->stack > 0);
        rt->stack--;
        break;

        case OPCODE_SETV:
        ASSERT(rt->stack > 0);
        b1 = rt_read_u8(rt);
        *rt_variable(rt, b1) =  rt->values[rt->stack-1];
        break;

        case OPCODE_PUSHV:
        if (!rt_check_stack(rt, 1)) break;
        b1 = rt_read_u8(rt);
        rt->values[rt->stack++] = *rt_variable(rt, b1);
        break;

        case OPCODE_PUSHI:
        if (!rt_check_stack(rt, 1)) break;
        i = rt_read_s64(rt);
        v1 = value_from_s64(i, rt->arena, &rt->err);
        rt->values[rt->stack++] = v1;
        break;

        case OPCODE_PUSHF:
        if (!rt_check_stack(rt, 1)) break;
        f = rt_read_f64(rt);
        v1 = value_from_f64(f, rt->arena, &rt->err);
        rt->values[rt->stack++] = v1;
        break;

        case OPCODE_PUSHS:
        if (!rt_check_stack(rt, 1)) break;
        s = rt_read_str(rt);
        v1 = value_from_str(s, rt->arena, &rt->err);
        rt->values[rt->stack++] = v1;
        break;

        case OPCODE_PUSHA:
        if (!rt_check_stack(rt, 1)) break;
        o = rt_read_u32(rt);
        v1 = value_empty_array(o, rt->arena, &rt->err);
        rt->values[rt->stack++] = v1;
        break;

        case OPCODE_PUSHM:
        if (!rt_check_stack(rt, 1)) break;
        o = rt_read_u32(rt);
        v1 = value_empty_map(o, rt->arena, &rt->err);
        rt->values[rt->stack++] = v1;
        break;

        case OPCODE_PUSHN:
        if (!rt_check_stack(rt, 1)) break;
        rt->values[rt->stack++] = VALUE_NONE;
        break;

        case OPCODE_PUSHT:
        if (!rt_check_stack(rt, 1)) break;
        rt->values[rt->stack++] = VALUE_TRUE;
        break;

        case OPCODE_PUSHFL:
        if (!rt_check_stack(rt, 1)) break;
        rt->values[rt->stack++] = VALUE_FALSE;
        break;

        case OPCODE_LEN:
        ASSERT(rt->stack > 0);
        v1 = rt->values[rt->stack-1];
        t = value_type(v1);
        if (t != TYPE_ARRAY && t != TYPE_MAP) {
            REPORT(&rt->err, "Invalid operation 'len' on non-aggregate value");
            rt->state = RUNTIME_ERROR;
            break;
        }
        v2 = value_from_s64(value_length(v1), rt->arena, &rt->err);
        rt->values[rt->stack-1] = v2;
        break;

        case OPCODE_NEG:
        ASSERT(rt->stack > 0);
        v1 = rt->values[rt->stack-1];
        v2 = value_neg(v1, rt->arena, &rt->err);
        rt->values[rt->stack-1] = v2;
        break;

        case OPCODE_EQL:
        ASSERT(rt->stack > 1);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = value_eql(v2, v1) ? VALUE_TRUE : VALUE_FALSE;
        rt->values[rt->stack++] = v3;
        break;

        case OPCODE_NQL:
        ASSERT(rt->stack > 1);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = value_nql(v2, v1) ? VALUE_TRUE : VALUE_FALSE;
        rt->values[rt->stack++] = v3;
        break;

        case OPCODE_LSS:
        ASSERT(rt->stack > 1);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = value_lower(v2, v1, &rt->err) ? VALUE_TRUE : VALUE_FALSE;
        rt->values[rt->stack++] = v3;
        break;

        case OPCODE_GRT:
        ASSERT(rt->stack > 1);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = value_greater(v2, v1, &rt->err) ? VALUE_TRUE : VALUE_FALSE;
        rt->values[rt->stack++] = v3;
        break;

        case OPCODE_ADD:
        ASSERT(rt->stack > 1);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = value_add(v2, v1, rt->arena, &rt->err);
        rt->values[rt->stack++] = v3;
        break;

        case OPCODE_SUB:
        ASSERT(rt->stack > 1);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = value_sub(v2, v1, rt->arena, &rt->err);
        rt->values[rt->stack++] = v3;
        break;

        case OPCODE_MUL:
        ASSERT(rt->stack > 1);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = value_mul(v2, v1, rt->arena, &rt->err);
        rt->values[rt->stack++] = v3;
        break;

        case OPCODE_DIV:
        ASSERT(rt->stack > 1);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = value_div(v2, v1, rt->arena, &rt->err);
        rt->values[rt->stack++] = v3;
        break;

        case OPCODE_MOD:
        ASSERT(rt->stack > 1);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = value_mod(v2, v1, rt->arena, &rt->err);
        rt->values[rt->stack++] = v3;
        break;

        case OPCODE_APPEND:
        ASSERT(rt->stack > 1);
        v2 = rt->values[--rt->stack];
        v1 = rt->values[rt->stack-1];
        value_append(v1, v2, rt->arena, &rt->err);
        break;

        case OPCODE_INSERT1:
        ASSERT(rt->stack > 2);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = rt->values[rt->stack-1];
        value_insert(v3, v1, v2, rt->arena, &rt->err);
        break;

        case OPCODE_INSERT2:
        ASSERT(rt->stack > 2);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = rt->values[rt->stack-1];
        value_insert(v2, v1, v3, rt->arena, &rt->err);
        break;

        case OPCODE_SELECT:
        ASSERT(rt->stack > 1);
        v1 = rt->values[--rt->stack];
        v2 = rt->values[--rt->stack];
        v3 = value_select(v2, v1, &rt->err);
        rt->values[rt->stack++] = v3;
        break;

        default:
        UNREACHABLE;
    }
}

WL_EvalResult wl_runtime_eval(WL_Runtime *rt)
{
    if (rt->state != RUNTIME_OUTPUT || rt->cur_output == rt->num_output) {

        switch (rt->state) {

            case RUNTIME_BEGIN:
            break;

            case RUNTIME_DONE:
            return (WL_EvalResult) { .type=WL_EVAL_DONE };

            case RUNTIME_ERROR:
            return (WL_EvalResult) { .type=WL_EVAL_ERROR };

            case RUNTIME_OUTPUT:
            rt->stack -= rt->num_output;
            break;

            case RUNTIME_SYSVAR:
            {
                ASSERT(rt->stack >= rt->stack_before_user);

                int pushed_by_user = rt->stack - rt->stack_before_user;
                if (pushed_by_user > 1) {
                    REPORT(&rt->err, "Invalid API usage");
                    rt->state = RUNTIME_ERROR;
                    return (WL_EvalResult) { .type=WL_EVAL_ERROR };
                }

                if (rt->stack == rt->stack_before_user) {
                    // User didn't push anything on the stack
                    if (!rt_check_stack(rt, 1))
                        return (WL_EvalResult) { .type=WL_EVAL_ERROR };
                    rt->values[rt->stack++] = VALUE_NONE;
                }

                rt_pop_frame(rt);
            }
            break;

            case RUNTIME_SYSCALL:
            ASSERT(rt->stack >= rt->stack_before_user);
            rt_pop_frame(rt);
            break;

            default:
            UNREACHABLE;
        }

        rt->state = RUNTIME_LOOP;

        do {

            step(rt);

            if (rt->err.yes)
                rt->state = RUNTIME_ERROR;

        } while (rt->state == RUNTIME_LOOP);

    }

    switch (rt->state) {

        case RUNTIME_BEGIN:
        case RUNTIME_LOOP:
        UNREACHABLE;

        case RUNTIME_DONE:
        break;

        case RUNTIME_ERROR:
        return (WL_EvalResult) { .type=WL_EVAL_ERROR };

        case RUNTIME_OUTPUT:
        {
            ASSERT(rt->cur_output < rt->num_output);

            Value v = rt->values[rt->stack - rt->num_output + rt->cur_output];
            Type type = value_type(v);

            String str;
            if (type == TYPE_STRING)
                str = value_to_str(v);
            else {
                int len = value_convert_to_str(v, rt->buf, SIZEOF(rt->buf));
                if (len > SIZEOF(rt->buf)) {
                    char *p = alloc(rt->arena, len, 1);
                    if (p == NULL) {
                        REPORT(&rt->err, "Out of memory");
                        rt->state = RUNTIME_ERROR;
                        return (WL_EvalResult) { .type=WL_EVAL_ERROR };
                    }
                    len = value_convert_to_str(v, p, len);
                    str = (String) { p, len };
                } else {
                    str = (String) { rt->buf, len };
                }
            }

            rt->cur_output++;
            return (WL_EvalResult) { .type=WL_EVAL_OUTPUT, .str={ str.ptr, str.len } };
        }

        case RUNTIME_SYSVAR:
        return (WL_EvalResult) { .type=WL_EVAL_SYSVAR, .str=(WL_String) { rt->str_for_user.ptr, rt->str_for_user.len } };

        case RUNTIME_SYSCALL:
        return (WL_EvalResult) { .type=WL_EVAL_SYSCALL, .str=(WL_String) { rt->str_for_user.ptr, rt->str_for_user.len } };
    }

    return (WL_EvalResult) { .type=WL_EVAL_DONE };
}

bool wl_streq(WL_String a, char *b, int blen)
{
    if (b == NULL) b = "";
    if (blen < 0) blen = strlen(b);
    return streq((String) { a.ptr, a.len }, (String) { b, blen });
}

int wl_arg_count(WL_Runtime *rt)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return -1;

    ASSERT(rt->num_frames > 0);
    return rt->frames[rt->num_frames-1].varbase - rt->vars; // TODO: is this right?
}

static Value user_arg(WL_Runtime *rt, int idx, Type type)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return -1;

    int tot = wl_arg_count(rt);
    if (idx < 0 || idx >= tot)
        return false;

    Value v = *rt_variable(rt, tot - idx - 1);
    if (value_type(v) != type)
        return VALUE_ERROR;

    return v;
}

bool wl_arg_none(WL_Runtime *rt, int idx)
{
    Value v = user_arg(rt, idx, TYPE_NONE);
    if (v == VALUE_ERROR)
        return false;
    return true;
}

bool wl_arg_bool(WL_Runtime *rt, int idx, bool *x)
{
    Value v = user_arg(rt, idx, TYPE_BOOL);
    if (v == VALUE_ERROR)
        return false;
    *x = (v == VALUE_TRUE);
    return true;
}

bool wl_arg_s64(WL_Runtime *rt, int idx, int64_t *x)
{
    Value v = user_arg(rt, idx, TYPE_INT);
    if (v == VALUE_ERROR)
        return false;
    *x = value_to_s64(v);
    return true;
}

bool wl_arg_f64(WL_Runtime *rt, int idx, double *x)
{
    Value v = user_arg(rt, idx, TYPE_FLOAT);
    if (v == VALUE_ERROR)
        return false;
    *x = value_to_f64(v);
    return true;
}

bool wl_arg_str(WL_Runtime *rt, int idx, WL_String *x)
{
    Value v = user_arg(rt, idx, TYPE_STRING);
    if (v == VALUE_ERROR)
        return false;
    String s = value_to_str(v);
    *x = (WL_String) { s.ptr, s.len };
    return true;
}

bool wl_arg_array(WL_Runtime *rt, int idx)
{
    Value v = user_arg(rt, idx, TYPE_ARRAY);
    if (v == VALUE_ERROR)
        return false;
    return true;
}

bool wl_arg_map(WL_Runtime *rt, int idx)
{
    Value v = user_arg(rt, idx, TYPE_MAP);
    if (v == VALUE_ERROR)
        return false;
    return true;
}

static Value user_peek(WL_Runtime *rt, int off, Type type)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return VALUE_ERROR;

    if (rt->stack + off < rt->stack_before_user || off >= 0)
        return VALUE_ERROR;

    Value v = rt->values[rt->stack + off];
    if (value_type(v) != type)
        return VALUE_ERROR;

    return v;
}

bool wl_peek_none(WL_Runtime *rt, int off)
{
    Value v = user_peek(rt, off, TYPE_NONE);
    if (v == VALUE_ERROR)
        return false;
    return true;
}

bool wl_peek_bool(WL_Runtime *rt, int off, bool *x)
{
    Value v = user_peek(rt, off, TYPE_BOOL);
    if (v == VALUE_ERROR)
        return false;
    *x = (v == VALUE_TRUE);
    return true;
}

bool wl_peek_s64(WL_Runtime *rt, int off, int64_t *x)
{
    Value v = user_peek(rt, off, TYPE_INT);
    if (v == VALUE_ERROR)
        return false;
    *x = value_to_s64(v);
    return true;
}

bool wl_peek_f64(WL_Runtime *rt, int off, double *x)
{
    Value v = user_peek(rt, off, TYPE_FLOAT);
    if (v == VALUE_ERROR)
        return false;
    *x = value_to_f64(v);
    return true;
}

bool wl_peek_str(WL_Runtime *rt, int off, WL_String *x)
{
    Value v = user_peek(rt, off, TYPE_STRING);
    if (v == VALUE_ERROR)
        return false;
    String s = value_to_str(v);
    *x = (WL_String) { s.ptr, s.len };
    return true;
}

bool wl_pop_any(WL_Runtime *rt)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return VALUE_ERROR;

    if (rt->stack == rt->stack_before_user)
        return false;

    ASSERT(rt->stack > 0);
    rt->stack--;
    return true;
}

static Value user_pop(WL_Runtime *rt, Type type)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return VALUE_ERROR;

    if (rt->stack == rt->stack_before_user)
        return VALUE_ERROR;

    ASSERT(rt->stack > 0);
    Value v = rt->values[rt->stack-1];
    if (value_type(v) != type)
        return VALUE_ERROR;

    rt->stack--;
    return v;
}

bool wl_pop_none(WL_Runtime *rt)
{
    Value v = user_pop(rt, TYPE_NONE);
    if (v == VALUE_ERROR)
        return false;
    return true;
}

bool wl_pop_bool(WL_Runtime *rt, bool *x)
{
    Value v = user_pop(rt, TYPE_BOOL);
    if (v == VALUE_ERROR)
        return false;
    *x = (v == VALUE_TRUE);
    return true;
}

bool wl_pop_s64(WL_Runtime *rt, int64_t *x)
{
    Value v = user_pop(rt, TYPE_INT);
    if (v == VALUE_ERROR)
        return false;
    *x = value_to_s64(v);
    return true;
}

bool wl_pop_f64(WL_Runtime *rt, double *x)
{
    Value v = user_pop(rt, TYPE_FLOAT);
    if (v == VALUE_ERROR)
        return false;
    *x = value_to_f64(v);
    return true;
}

bool wl_pop_str(WL_Runtime *rt, WL_String *x)
{
    Value v = user_pop(rt, TYPE_STRING);
    if (v == VALUE_ERROR)
        return false;
    String s = value_to_str(v);
    *x = (WL_String) { s.ptr, s.len };
    return true;
}

void wl_push_none(WL_Runtime *rt)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return;

    if (!rt_check_stack(rt, 1))
        return;

    rt->values[rt->stack++] = VALUE_NONE;
}

void wl_push_true(WL_Runtime *rt)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return;

    if (!rt_check_stack(rt, 1))
        return;

    rt->values[rt->stack++] = VALUE_TRUE;
}

void wl_push_false(WL_Runtime *rt)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return;

    if (!rt_check_stack(rt, 1))
        return;

    rt->values[rt->stack++] = VALUE_FALSE;
}

void wl_push_s64(WL_Runtime *rt, int64_t x)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return;

    if (!rt_check_stack(rt, 1))
        return;

    Value v = value_from_s64(x, rt->arena, &rt->err);
    if (v == VALUE_ERROR) {
        rt->state = RUNTIME_ERROR;
        return;
    }

    rt->values[rt->stack++] = v;
}

void wl_push_f64(WL_Runtime *rt, double x)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return;

    if (!rt_check_stack(rt, 1))
        return;

    Value v = value_from_f64(x, rt->arena, &rt->err);
    if (v == VALUE_ERROR) {
        rt->state = RUNTIME_ERROR;
        return;
    }

    rt->values[rt->stack++] = v;
}

void wl_push_str(WL_Runtime *rt, WL_String x)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return;

    if (!rt_check_stack(rt, 1))
        return;

    Value v = value_from_str((String) { x.ptr, x.len }, rt->arena, &rt->err);
    if (v == VALUE_ERROR) {
        rt->state = RUNTIME_ERROR;
        return;
    }

    rt->values[rt->stack++] = v;
}

void wl_push_array(WL_Runtime *rt, int cap)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return;

    if (!rt_check_stack(rt, 1))
        return;

    Value v = value_empty_array(cap, rt->arena, &rt->err);
    if (v == VALUE_ERROR) {
        rt->state = RUNTIME_ERROR;
        return;
    }

    rt->values[rt->stack++] = v;
}

void wl_push_map(WL_Runtime *rt, int cap)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return;

    if (!rt_check_stack(rt, 1))
        return;

    Value v = value_empty_map(cap, rt->arena, &rt->err);
    if (v == VALUE_ERROR) {
        rt->state = RUNTIME_ERROR;
        return;
    }

    rt->values[rt->stack++] = v;
}

void wl_push_arg(WL_Runtime *rt, int idx)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return;

    if (!rt_check_stack(rt, 1))
        return;

    int tot = wl_arg_count(rt);
    if (idx < 0 || idx >= tot) {
        REPORT(&rt->err, "Invalid API usagge");
        rt->state = RUNTIME_ERROR;
        return;
    }

    rt->values[rt->stack++] = *rt_variable(rt, tot - idx - 1);
}

void wl_insert(WL_Runtime *rt)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return;

    if (rt->stack - rt->stack_before_user < 3) {
        REPORT(&rt->err, "Invalid API usagge");
        rt->state = RUNTIME_ERROR;
        return;
    }

    Value key = rt->values[--rt->stack];
    Value val = rt->values[--rt->stack];
    Value set = rt->values[rt->stack-1];

    if (!value_insert(set, key, val, rt->arena, &rt->err)) {
        rt->state = RUNTIME_ERROR;
        return;
    }
}

void wl_append(WL_Runtime *rt)
{
    if (rt->state != RUNTIME_SYSVAR &&
        rt->state != RUNTIME_SYSCALL)
        return;

     if (rt->stack - rt->stack_before_user < 2) {
        REPORT(&rt->err, "Invalid API usagge");
        rt->state = RUNTIME_ERROR;
        return;
    }

    Value val = rt->values[--rt->stack];
    Value set = rt->values[rt->stack-1];

    if (!value_append(set, val, rt->arena, &rt->err)) {
        rt->state = RUNTIME_ERROR;
        return;
    }
}

void wl_runtime_dump(WL_Runtime *rt)
{
    for (int i = 0; i < rt->num_frames; i++) {
        printf("=== frame %d ===\n", i);
        
        Frame *frame = &rt->frames[i];

        int num_vars;
        if (i+1 < rt->num_frames)
            num_vars = frame->varbase - rt->frames[i+1].varbase;
        else
            num_vars = frame->varbase - rt->vars;

        for (int j = 0; j < num_vars; j++) {
            printf("  %d = ", j);
            value_print(rt->values[frame->varbase - j]);
        }
    }
    printf("===============\n");
}
