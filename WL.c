#include "WL.h"

////////////////////////////////////////////////////////////////////////////////////////
// src/includes.h
////////////////////////////////////////////////////////////////////////////////////////

#ifndef WL_INCLUDES_INCLUDED
#define WL_INCLUDES_INCLUDED

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifndef _WIN32
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#endif

#endif // WL_INCLUDES_INCLUDED

////////////////////////////////////////////////////////////////////////////////////////
// src/basic.h
////////////////////////////////////////////////////////////////////////////////////////

#ifndef WL_BASIC_INCLUDED
#define WL_BASIC_INCLUDED

#ifndef WL_AMALGAMATION
#include "public.h"
#endif

typedef struct {
    char *ptr;
    int   len;
} String;

#ifdef _WIN32
#define LLU "llu"
#define LLD "lld"
#else
#define LLU "lu"
#define LLD "ld"
#endif

#define S(X) (String) { (X), (int) sizeof(X)-1 }

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

#define COUNT(X) (int) (sizeof(X) / sizeof((X)[0]))

bool is_space(char c);
bool is_digit(char c);
bool is_alpha(char c);
bool is_printable(char c);
char to_lower(char c);
bool is_hex_digit(char c);
int  hex_digit_to_int(char c);

bool streq(String a, String b);
bool streqcase(String a, String b);
String copystr(String s, WL_Arena *a);

void *alloc(WL_Arena *a, int len, int align);
bool grow_alloc(WL_Arena *a, char *p, int new_len);

#endif // WL_BASIC_INCLUDED

////////////////////////////////////////////////////////////////////////////////////////
// src/basic.c
////////////////////////////////////////////////////////////////////////////////////////


#ifndef WL_AMALGAMATION
#include "includes.h"
#include "basic.h"
#include "public.h"
#endif

bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_printable(char c)
{
    return c >= ' ' && c <= '~';
}

bool is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

char to_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 10;
    return c;
}

int hex_digit_to_int(char c)
{
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return c - '0';
}


bool streq(String a, String b)
{
    if (a.len != b.len)
        return false;
    for (int i = 0; i < a.len; i++)
        if (a.ptr[i] != b.ptr[i])
            return false;
    return true;
}

bool streqcase(String a, String b)
{
    if (a.len != b.len)
        return false;
    for (int i = 0; i < a.len; i++)
        if (to_lower(a.ptr[i]) != to_lower(b.ptr[i]))
            return false;
    return true;
}

void *alloc(WL_Arena *a, int len, int align)
{
    int pad = -(intptr_t) (a->ptr + a->cur) & (align-1);
    if (a->len - a->cur < len + pad)
        return NULL;
    void *ret = a->ptr + a->cur + pad;
    a->cur += pad + len;
    return ret;
}

bool grow_alloc(WL_Arena *a, char *p, int new_len)
{
    int new_cur = (p - a->ptr) + new_len;
    if (new_cur > a->len)
        return false;
    a->cur = new_cur;
    return true;
}

String copystr(String s, WL_Arena *a)
{
    char *p = alloc(a, s.len, 1);
    if (p == NULL)
        return (String) { NULL, 0 };
    memcpy(p, s.ptr, s.len);
    return (String) { p, s.len };
}

////////////////////////////////////////////////////////////////////////////////////////
// src/file.h
////////////////////////////////////////////////////////////////////////////////////////

#ifndef WL_FILE_INCLUDED
#define WL_FILE_INCLUDED

#ifndef WL_AMALGAMATION
#include "includes.h"
#include "basic.h"
#endif

#ifdef _WIN32
typedef HANDLE File;
#else
typedef int File;
#endif

int  file_open(String path, File *handle, int *size);
void file_close(File file);
int  file_read(File file, char *dst, int max);
int  file_read_all(String path, String *dst);

#endif // WL_FILE_INCLUDED

////////////////////////////////////////////////////////////////////////////////////////
// src/file.c
////////////////////////////////////////////////////////////////////////////////////////

#ifndef WL_AMALGAMATION
#include "includes.h"
#include "file.h"
#endif

int file_open(String path, File *handle, int *size)
{
    char zt[1<<10];
    if (path.len >= COUNT(zt))
        return -1;
    memcpy(zt, path.ptr, path.len);
    zt[path.len] = '\0';

#ifdef _WIN32
    *handle = CreateFileA(
        zt,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (*handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND ||
            error == ERROR_ACCESS_DENIED)
            return 1;
        return -1;
    }
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(*handle, &fileSize)) {
        CloseHandle(*handle);
        return -1;
    }
    if (fileSize.QuadPart > INT_MAX) {
        CloseHandle(*handle);
        return -1;
    }
    *size = (int) fileSize.QuadPart;
#else
    *handle = open(zt, O_RDONLY);
    if (*handle < 0) {
        if (errno == ENOENT)
            return 1;
        return -1;
    }
    struct stat info;
    if (fstat(*handle, &info) < 0) {
        close(*handle);
        return -1;
    }
    if (S_ISDIR(info.st_mode)) {
        close(*handle);
        return 1;
    }
    if (info.st_size > INT_MAX) {
        close(*handle);
        return -1;
    }
    *size = (int) info.st_size;
#endif
    return 0;
}

void file_close(File file)
{
#ifdef _WIN32
	CloseHandle(file);
#else
	close(file);
#endif
}

int file_read(File file, char *dst, int max)
{
#ifdef _WIN32
    DWORD num;
    BOOL ok = ReadFile(file, dst, max, &num, NULL);
    if (!ok)
        return -1;
    return (int) num;
#else
    return read(file, dst, max);
#endif
}

int file_read_all(String path, String *dst)
{
    int len;
    File handle;
    if (file_open(path, &handle, &len) < 0)
        return -1;

    char *ptr = malloc(len+1);
    if (ptr == NULL) {
        file_close(handle);
        return -1;
    }

    for (int copied = 0; copied < len; ) {
        int ret = file_read(handle, ptr + copied, len - copied);
        if (ret <= 0) {
            free(ptr);
            file_close(handle);
            return -1;
        }
        copied += ret;
    }

    *dst = (String) { ptr, len };
    file_close(handle);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// src/parse.h
////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////
// src/parse.c
////////////////////////////////////////////////////////////////////////////////////////


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

////////////////////////////////////////////////////////////////////////////////////////
// src/assemble.h
////////////////////////////////////////////////////////////////////////////////////////

#ifndef WL_ASSEMBLE_INCLUDED
#define WL_ASSEMBLE_INCLUDED

#ifndef WL_AMALGAMATION
#include "public.h"
#include "parse.h"
#endif

enum {
    OPCODE_NOPE    = 0x00,
    OPCODE_EXIT    = 0x23,
    OPCODE_GROUP   = 0x25,
    OPCODE_GPOP    = 0x26,
    OPCODE_GPRINT  = 0x27,
    OPCODE_GTRUNC  = 0x28,
    OPCODE_GCOALESCE = 0x29,
    OPCODE_GOVERWRITE = 0x2A,
    OPCODE_GPACK   = 0x2B,
    OPCODE_PUSHI   = 0x01,
    OPCODE_PUSHF   = 0x02,
    OPCODE_PUSHS   = 0x03,
    OPCODE_PUSHV   = 0x04,
    OPCODE_PUSHA   = 0x05,
    OPCODE_PUSHM   = 0x06,
    OPCODE_PUSHN   = 0x21,
    OPCODE_POP     = 0x07,
    OPCODE_NEG     = 0x08,
    OPCODE_EQL     = 0x09,
    OPCODE_NQL     = 0x0A,
    OPCODE_LSS     = 0x0B,
    OPCODE_GRT     = 0x0C,
    OPCODE_ADD     = 0x0D,
    OPCODE_SUB     = 0x0E,
    OPCODE_MUL     = 0x0F,
    OPCODE_DIV     = 0x10,
    OPCODE_MOD     = 0x11,
    OPCODE_SETV    = 0x12,
    OPCODE_JUMP    = 0x13,
    OPCODE_JIFP    = 0x14,
    OPCODE_CALL    = 0x15,
    OPCODE_RET     = 0x16,
    OPCODE_APPEND  = 0x17,
    OPCODE_INSERT1 = 0x18,
    OPCODE_INSERT2 = 0x19,
    OPCODE_SELECT  = 0x20,
    OPCODE_PRINT   = 0x24,
    OPCODE_SYSVAR  = 0x2C,
    OPCODE_SYSCALL = 0x2D,
    OPCODE_FOR     = 0x2E,
    OPCODE_PUSHT   = 0x2F,
    OPCODE_PUSHFL  = 0x30,
};

typedef struct {
    WL_Program program;
    int errlen;
} AssembleResult;

int parse_program_header(WL_Program p, String *code, String *data, char *errbuf, int errmax);
void  print_program(WL_Program program);
char *print_instruction(char *p, char *data);
AssembleResult assemble(Node *root, WL_Arena *arena, char *errbuf, int errmax);

#endif // WL_ASSEMBLE_INCLUDED

////////////////////////////////////////////////////////////////////////////////////////
// src/assemble.c
////////////////////////////////////////////////////////////////////////////////////////


#ifndef WL_AMALGAMATION
#include "includes.h"
#include "parse.h"
#include "assemble.h"
#endif

#define MAX_SCOPES  32
#define MAX_SYMBOLS 1024
#define MAX_DEPTH 128

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
    SCOPE_HTML,
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
    bool   err;
} OutputBuffer;

typedef struct {

    WL_Arena *a;

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

        int   new_cap = MAX(out->len + len, 2 * out->cap);
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

int append_s64(OutputBuffer *out, int64_t x)
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

bool global_scope(Assembler *a)
{
    return parent_scope(a)->type == SCOPE_GLOBAL;
}

Symbol *find_symbol_in_local_scope(Assembler *a, String name)
{
    if (name.len == 0)
        return NULL;

    Scope *scope = &a->scopes[a->num_scopes-1];
    for (int i = a->num_syms-1; i >= scope->sym_base; i--)
        if (streq(a->syms[i].name, name))
            return &a->syms[i];
    return NULL;
}

Symbol *find_symbol_in_function(Assembler *a, String name)
{
    if (name.len == 0)
        return NULL;

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

bool is_expr(Node *node)
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
        case NODE_VALUE_NONE:
        case NODE_VALUE_TRUE:
        case NODE_VALUE_FALSE:
        case NODE_VALUE_VAR:
        case NODE_VALUE_SYSVAR:
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

void assemble_statement(Assembler *a, Node *node, bool pop_expr);

typedef struct {
    OutputBuffer tmp;
} HTMLAssembler;

void write_buffered_html(Assembler *a, HTMLAssembler *ha)
{
    if (ha->tmp.len == 0)
        return;

    int off = add_string_literal(a, (String) { ha->tmp.ptr, ha->tmp.len });
    append_u8(&a->out, OPCODE_PUSHS);
    append_u32(&a->out, off);
    append_u32(&a->out, ha->tmp.len);

    free(ha->tmp.ptr);
    ha->tmp.ptr = NULL;
    ha->tmp.len = 0;
    ha->tmp.cap = 0;
}

void assemble_html_2(Assembler *a, HTMLAssembler *ha, Node *node)
{
    append_u8(&ha->tmp, '<');
    append_mem(&ha->tmp, node->tagname.ptr, node->tagname.len);

    Node *attr = node->params;
    while (attr) {

        String name  = attr->attr_name;
        Node  *value = attr->attr_value;

        append_u8(&ha->tmp, ' ');
        append_mem(&ha->tmp, name.ptr, name.len);

        if (value) {
            append_u8(&ha->tmp, '=');
            append_u8(&ha->tmp, '"');

            if (value->type == NODE_VALUE_STR) {
                append_mem(&ha->tmp,
                    value->sval.ptr, // TODO: escape
                    value->sval.len
                );
            } else {
                write_buffered_html(a, ha);
                assemble_statement(a, value, false);
            }

            append_u8(&ha->tmp, '"');
        }
        attr = attr->next;
    }

    if (node->no_body) {
        append_u8(&ha->tmp, ' ');
        append_u8(&ha->tmp, '/');
        append_u8(&ha->tmp, '>');
    } else {

        append_u8(&ha->tmp, '>');

        Node *child = node->child;
        while (child) {
            if (child->type == NODE_VALUE_STR)
                append_mem(&ha->tmp, child->sval.ptr, child->sval.len);
            else if (child->type == NODE_VALUE_HTML)
                assemble_html_2(a, ha, child);
            else {
                write_buffered_html(a, ha);
                assemble_statement(a, child, false);
            }
            child = child->next;
        }

        append_u8(&ha->tmp, '<');
        append_u8(&ha->tmp, '/');
        append_mem(&ha->tmp, node->tagname.ptr, node->tagname.len);
        append_u8(&ha->tmp, '>');
    }
}

void assemble_html(Assembler *a, Node *node)
{
    HTMLAssembler ha = {
        .tmp={.ptr=NULL,.len=0,.cap=0,.err=false},
    };
    assemble_html_2(a, &ha, node);
    write_buffered_html(a, &ha);
}

void assemble_expr(Assembler *a, Node *node, int num_results)
{
    switch (node->type) {

        default:
        assert(0);
        break;

        case NODE_FUNC_CALL:
        {
            Node *func = node->left;
            Node *args = node->right;

            append_u8(&a->out, OPCODE_GROUP);

            int arg_count = 0;
            Node *arg = args;
            while (arg) {
                assemble_expr(a, arg, 1);
                arg_count++;
                arg = arg->next;
            }

            if (func->type == NODE_VALUE_SYSVAR) {

                String name = func->sval;
                int off = add_string_literal(a, name);

                append_u8(&a->out, OPCODE_SYSCALL);
                append_u32(&a->out, off);
                append_u32(&a->out, name.len);

            } else {

                assert(func->type == NODE_VALUE_VAR);

                append_u8(&a->out, OPCODE_CALL);
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

            if (num_results == 0)
                append_u8(&a->out, OPCODE_GPOP);
            else if (num_results != -1) {
                append_u8(&a->out, OPCODE_GTRUNC);
                append_u32(&a->out, num_results);
            }

            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_OPER_POS:
        assemble_expr(a, node->left, num_results);
        break;

        case NODE_OPER_NEG:
        assemble_expr(a, node->left, 1);
        append_u8(&a->out, OPCODE_NEG);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_OPER_EQL:
        assemble_expr(a, node->left, 1);
        assemble_expr(a, node->right, 1);
        append_u8(&a->out, OPCODE_EQL);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_OPER_NQL:
        assemble_expr(a, node->left, 1);
        assemble_expr(a, node->right, 1);
        append_u8(&a->out, OPCODE_NQL);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_OPER_LSS:
        assemble_expr(a, node->left, 1);
        assemble_expr(a, node->right, 1);
        append_u8(&a->out, OPCODE_LSS);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_OPER_GRT:
        assemble_expr(a, node->left, 1);
        assemble_expr(a, node->right, 1);
        append_u8(&a->out, OPCODE_GRT);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_OPER_ADD:
        assemble_expr(a, node->left, 1);
        assemble_expr(a, node->right, 1);
        append_u8(&a->out, OPCODE_ADD);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_OPER_SUB:
        assemble_expr(a, node->left, 1);
        assemble_expr(a, node->right, 1);
        append_u8(&a->out, OPCODE_SUB);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_OPER_MUL:
        assemble_expr(a, node->left, 1);
        assemble_expr(a, node->right, 1);
        append_u8(&a->out, OPCODE_MUL);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_OPER_DIV:
        assemble_expr(a, node->left, 1);
        assemble_expr(a, node->right, 1);
        append_u8(&a->out, OPCODE_DIV);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_OPER_MOD:
        assemble_expr(a, node->left, 1);
        assemble_expr(a, node->right, 1);
        append_u8(&a->out, OPCODE_MOD);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_VALUE_INT:
        append_u8(&a->out, OPCODE_PUSHI);
        append_s64(&a->out, node->ival);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_VALUE_FLOAT:
        append_u8 (&a->out, OPCODE_PUSHF);
        append_f64(&a->out, node->dval);

        if (num_results == 0)
            append_u8(&a->out, OPCODE_POP);
        else if (num_results != -1 && num_results != 1) {
            append_u8(&a->out, OPCODE_GROUP);
            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, num_results-1);
            append_u8(&a->out, OPCODE_GCOALESCE);
        }
        break;

        case NODE_VALUE_STR:
        {
            int off = add_string_literal(a, node->sval);
            append_u8(&a->out, OPCODE_PUSHS);
            append_u32(&a->out, off);
            append_u32(&a->out, node->sval.len);

            if (num_results == 0)
                append_u8(&a->out, OPCODE_POP);
            else if (num_results != -1 && num_results != 1) {
                append_u8(&a->out, OPCODE_GROUP);
                append_u8(&a->out, OPCODE_GTRUNC);
                append_u32(&a->out, num_results-1);
                append_u8(&a->out, OPCODE_GCOALESCE);
            }
        }
        break;

        case NODE_VALUE_NONE:
        {
            append_u8(&a->out, OPCODE_PUSHN);
        }
        break;

        case NODE_VALUE_TRUE:
        {
            append_u8(&a->out, OPCODE_PUSHT);
        }
        break;

        case NODE_VALUE_FALSE:
        {
            append_u8(&a->out, OPCODE_PUSHFL);
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

            if (num_results == 0)
                append_u8(&a->out, OPCODE_POP);
            else if (num_results != -1 && num_results != 1) {
                append_u8(&a->out, OPCODE_GROUP);
                append_u8(&a->out, OPCODE_GTRUNC);
                append_u32(&a->out, num_results-1);
                append_u8(&a->out, OPCODE_GCOALESCE);
            }
        }
        break;

        case NODE_VALUE_SYSVAR:
        {
            String name = node->sval;
            int off = add_string_literal(a, name);

            append_u8(&a->out, OPCODE_SYSVAR);
            append_u32(&a->out, off);
            append_u32(&a->out, name.len);

            if (num_results == 0)
                append_u8(&a->out, OPCODE_POP);
            else if (num_results != -1 && num_results != 1) {
                append_u8(&a->out, OPCODE_GROUP);
                append_u8(&a->out, OPCODE_GTRUNC);
                append_u32(&a->out, num_results-1);
                append_u8(&a->out, OPCODE_GCOALESCE);
            }
        }
        break;

        case NODE_VALUE_HTML:
        {
            if (num_results != -1)
                append_u8(&a->out, OPCODE_GROUP);

            assemble_html(a, node);

            if (num_results != -1) {

                append_u8(&a->out, OPCODE_GPACK);

                if (num_results > 1) {
                    append_u8(&a->out, OPCODE_GTRUNC);
                    append_u32(&a->out, num_results-1);
                    append_u8(&a->out, OPCODE_GCOALESCE);
                }
            }
        }
        break;

        case NODE_VALUE_ARRAY:
        {
            append_u8(&a->out, OPCODE_PUSHA);
            append_u32(&a->out, count_nodes(node->child));

            Node *child = node->child;
            while (child) {
                assemble_expr(a, child, 1);
                append_u8(&a->out, OPCODE_APPEND);
                child = child->next;
            }

            if (num_results == 0)
                append_u8(&a->out, OPCODE_POP);
            else if (num_results != -1) {
                append_u8(&a->out, OPCODE_GROUP);
                append_u8(&a->out, OPCODE_GTRUNC);
                append_u32(&a->out, num_results-1);
                append_u8(&a->out, OPCODE_GCOALESCE);
            }
        }
        break;

        case NODE_VALUE_MAP:
        {
            append_u8(&a->out, OPCODE_PUSHM);
            append_u32(&a->out, count_nodes(node->child));

            Node *child = node->child;
            while (child) {
                assemble_expr(a, child, 1);
                assemble_expr(a, child->key, 1);
                append_u8(&a->out, OPCODE_INSERT1);
                child = child->next;
            }

            if (num_results == 0)
                append_u8(&a->out, OPCODE_POP);
            else if (num_results != -1) {
                append_u8(&a->out, OPCODE_GROUP);
                append_u8(&a->out, OPCODE_GTRUNC);
                append_u32(&a->out, num_results-1);
                append_u8(&a->out, OPCODE_GCOALESCE);
            }
        }
        break;

        case NODE_SELECT:
        {
            Node *set = node->left;
            Node *key = node->right;

            assemble_expr(a, set, 1);
            assemble_expr(a, key, 1);
            append_u8(&a->out, OPCODE_SELECT);

            if (num_results == 0)
                append_u8(&a->out, OPCODE_POP);
            else if (num_results != -1) {
                append_u8(&a->out, OPCODE_GROUP);
                append_u8(&a->out, OPCODE_GTRUNC);
                append_u32(&a->out, num_results-1);
                append_u8(&a->out, OPCODE_GCOALESCE);
            }
        }
        break;

        case NODE_NESTED:
        assemble_expr(a, node->left, num_results);
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

                assemble_expr(a, src, 1);
                append_u8(&a->out, OPCODE_SETV);
                append_u8(&a->out, sym->off);

            } else if (dst->type == NODE_SELECT) {

                assemble_expr(a, src, 1);
                assemble_expr(a, dst->left, 1);
                assemble_expr(a, dst->right, 1);
                append_u8(&a->out, OPCODE_INSERT2);

            } else {

                assembler_report(a, "Assignment left side can't be assigned to");
                return;
            }

            if (num_results == 0)
                append_u8(&a->out, OPCODE_POP);
            else if (num_results != -1 && num_results != 1) {
                append_u8(&a->out, OPCODE_GROUP);
                append_u8(&a->out, OPCODE_GTRUNC);
                append_u32(&a->out, num_results-1);
                append_u8(&a->out, OPCODE_GCOALESCE);
            }
        }
        break;
    }
}

void assemble_statement(Assembler *a, Node *node, bool pop_expr)
{
    switch (node->type) {

        case NODE_INCLUDE:
        {
            assert(node->include_root);
            assemble_statement(a, node->include_root, pop_expr);
        }
        break;

        case NODE_PRINT:
        {
            append_u8(&a->out, OPCODE_GROUP);
            assemble_expr(a, node->left, -1);
            append_u8(&a->out, OPCODE_GPRINT);
            append_u8(&a->out, OPCODE_GPOP);
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

            int arg_count = count_nodes(node->func_args);

            append_u8(&a->out, OPCODE_GTRUNC);
            append_u32(&a->out, arg_count);

            append_u8(&a->out, OPCODE_GTRUNC);
            int p = append_u32(&a->out, 0);

            Node *arg = node->func_args;
            int idx = 0;
            while (arg) {

                int off = declare_variable(a, arg->sval);
                if (off < 0) return;

                assert(off == idx);

                idx++;
                arg = arg->next;
            }

            append_u8(&a->out, OPCODE_GROUP);

            if (is_expr(node->func_body)) {
                assemble_expr(a, node->func_body, -1);
            } else {
                assemble_statement(a, node->func_body, true);
                append_u8(&a->out, OPCODE_PUSHN);
            }

            append_u8(&a->out, OPCODE_GOVERWRITE);
            append_u8(&a->out, OPCODE_RET);

            patch_u32(&a->out, p, a->scopes[a->num_scopes-1].max_vars);

            ret = pop_scope(a);
            if (ret < 0) return;

            patch_with_current_offset(&a->out, p1);
        }
        break;

        case NODE_VAR_DECL:
        {
            int off = declare_variable(a, node->var_name);
            if (off < 0) return;

            if (node->var_value)
                assemble_expr(a, node->var_value, 1);
            else
                append_u8(&a->out, OPCODE_PUSHN);

            append_u8(&a->out, OPCODE_SETV);
            append_u8(&a->out, off);
        }
        break;

        case NODE_BLOCK:
        case NODE_GLOBAL_BLOCK:
        {
            if (node->type == NODE_BLOCK) {
                int ret = push_scope(a, SCOPE_BLOCK);
                if (ret < 0) return;
            }

            Node *stmt = node->left;
            while (stmt) {
                assemble_statement(a, stmt, pop_expr);
                stmt = stmt->next;
            }

            if (node->type == NODE_BLOCK) {
                int ret = pop_scope(a);
                if (ret < 0) return;
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

                assemble_expr(a, node->cond, 1);

                append_u8(&a->out, OPCODE_JIFP);
                int p1 = append_u32(&a->out, 0);

                int ret = push_scope(a, SCOPE_IF);
                if (ret < 0) return;

                assemble_statement(a, node->left, pop_expr);

                ret = pop_scope(a);
                if (ret < 0) return;

                append_u8(&a->out, OPCODE_JUMP);
                int p2 = append_u32(&a->out, 0);

                patch_with_current_offset(&a->out, p1);

                ret = push_scope(a, SCOPE_ELSE);
                if (ret < 0) return;

                assemble_statement(a, node->right, pop_expr);

                ret = pop_scope(a);
                if (ret < 0) return;

                patch_with_current_offset(&a->out, p2);

            } else {

                assemble_expr(a, node->cond, 1);

                append_u8(&a->out, OPCODE_JIFP);
                int p1 = append_u32(&a->out, 0);

                int ret = push_scope(a, SCOPE_IF);
                if (ret < 0) return;

                assemble_statement(a, node->left, pop_expr);

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

            assemble_expr(a, node->cond, 1);

            append_u8(&a->out, OPCODE_JIFP);
            int p = append_u32(&a->out, 0);

            int ret = push_scope(a, SCOPE_WHILE);
            if (ret < 0) return;

            assemble_statement(a, node->left, pop_expr);

            ret = pop_scope(a);
            if (ret < 0) return;

            append_u8(&a->out, OPCODE_JUMP);
            append_u32(&a->out, start);

            patch_with_current_offset(&a->out, p);
        }
        break;

        case NODE_FOR:
        {
            int ret = push_scope(a, SCOPE_FOR);
            if (ret < 0) return;

            int var_1 = declare_variable(a, node->for_var1);
            int var_2 = declare_variable(a, node->for_var2);
            int var_3 = declare_variable(a, (String) { NULL, 0 });

            assemble_expr(a, node->for_set, 1);
            append_u8(&a->out, OPCODE_SETV);
            append_u8(&a->out, var_3);

            append_u8(&a->out, OPCODE_PUSHI);
            append_s64(&a->out, 0);
            append_u8(&a->out, OPCODE_SETV);
            append_u8(&a->out, var_2);

            int start = append_u8(&a->out, OPCODE_FOR);
            append_u8(&a->out, var_3);
            append_u8(&a->out, var_1);
            append_u8(&a->out, var_2);
            int p = append_u32(&a->out, 0);

            assemble_statement(a, node->left, pop_expr);

            append_u8(&a->out, OPCODE_JUMP);
            append_u32(&a->out, start);

            patch_with_current_offset(&a->out, p);

            ret = pop_scope(a);
            if (ret < 0) return;
        }
        break;

        default:
        assemble_expr(a, node, pop_expr ? 0 : -1);
        break;
    }
}

typedef struct {
    uint32_t magic;
    uint32_t code_size;
    uint32_t data_size;
} Header;

AssembleResult assemble(Node *root, WL_Arena *arena, char *errbuf, int errmax)
{
    Assembler a = {0};
    a.errbuf = errbuf;
    a.errmax = errmax;
    a.a = arena;

    int ret = push_scope(&a, SCOPE_GLOBAL);
    if (ret < 0)
        return (AssembleResult) { (WL_Program) {0}, a.errlen };

    append_u8(&a.out, OPCODE_GROUP);

    append_u8(&a.out, OPCODE_GTRUNC);
    int p = append_u32(&a.out, 0);

    append_u8(&a.out, OPCODE_GROUP);

    assemble_statement(&a, root, false);

    append_u8(&a.out, OPCODE_GPRINT);
    append_u8(&a.out, OPCODE_GPOP);

    append_u8(&a.out, OPCODE_GPOP);
    append_u8(&a.out, OPCODE_EXIT);

    patch_u32(&a.out, p, a.scopes[a.num_scopes-1].max_vars);

    ret = pop_scope(&a);
    if (ret < 0)
        return (AssembleResult) { (WL_Program) {0}, a.errlen };

    OutputBuffer out = {0};
    append_u32(&out, 0xFEEDBEEF);    // magic
    append_u32(&out, a.out.len);     // code size
    append_u32(&out, a.strings_len); // data size
    append_mem(&out, a.out.ptr, a.out.len);
    append_mem(&out, a.strings, a.strings_len);

    free(a.out.ptr);
    return (AssembleResult) { (WL_Program) { out.ptr, out.len }, a.errlen };
}

int parse_program_header(WL_Program p, String *code, String *data, char *errbuf, int errmax)
{
    if ((uint32_t) p.len < 3 * sizeof(uint32_t)) {
        snprintf(errbuf, errmax, "Invalid program");
        return -1;
    }

    uint32_t magic;
    uint32_t code_size;
    uint32_t data_size;
    memcpy(&magic,     p.ptr + 0, sizeof(uint32_t));
    memcpy(&code_size, p.ptr + 4, sizeof(uint32_t));
    memcpy(&data_size, p.ptr + 8, sizeof(uint32_t));

    if (magic != 0xFEEDBEEF) {
        snprintf(errbuf, errmax, "Invalid program");
        return -1;
    }

    if (code_size + data_size + 3 * sizeof(uint32_t) != (uint32_t) p.len) {
        snprintf(errbuf, errmax, "Invalid program");
        return -1;
    }

    *code = (String) { p.ptr + 3 * sizeof(uint32_t),             code_size };
    *data = (String) { p.ptr + 3 * sizeof(uint32_t) + code_size, data_size };
    return 0;
}

void print_program(WL_Program program)
{
    String code;
    String data;

    char err[128];
    if (parse_program_header(program, &code, &data, err, COUNT(err)) < 0) {
        printf("%s\n", err);
        return;
    }

    char *p = code.ptr;
    for (;;) {
        printf(" %-3d: ", (int) (p - code.ptr));
        p = print_instruction(p, data.ptr);
        printf("\n");
        if (p == code.ptr + code.len)
            break;
    }
}

char *print_instruction(char *p, char *data)
{
    switch (*(p++)) {

        default:
        printf("(unknown opcode 0x%x)", *p);
        break;

        case OPCODE_NOPE:
        printf("NOPE");
        break;

        case OPCODE_EXIT:
        printf("EXIT");
        break;

        case OPCODE_GROUP:
        {
            printf("GROUP");
        }
        break;

        case OPCODE_GPOP:
        {
            printf("GPOP");
        }
        break;

        case OPCODE_GPRINT:
        {
            printf("GPRINT");
        }
        break;

        case OPCODE_GTRUNC:
        {
            uint32_t off;
            memcpy(&off, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            printf("GTRUNC %u", off);
        }
        break;

        case OPCODE_GCOALESCE:
        {
            printf("GCOALESCE");
        }
        break;

        case OPCODE_GOVERWRITE:
        {
            printf("GOVERWRITE");
        }
        break;

        case OPCODE_GPACK:
        {
            printf("GPACK");
        }
        break;

        case OPCODE_PUSHI:
        {
            int64_t x;
            memcpy(&x, p, sizeof(int64_t));
            p += sizeof(int64_t);

            printf("PUSHI %" LLU, x);
        }
        break;

        case OPCODE_PUSHF:
        {
            double x;
            memcpy(&x, p, sizeof(double));
            p += sizeof(double);

            printf("PUSHF %lf", x);
        }
        break;

        case OPCODE_PUSHS:
        {
            uint32_t off;
            memcpy(&off, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            uint32_t len;
            memcpy(&len, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            printf("PUSHS \"%.*s\"", (int) len, (char*) data + off);
        }
        break;

        case OPCODE_PUSHV:
        {
            uint8_t idx;
            memcpy(&idx, p, sizeof(uint8_t));
            p += sizeof(uint8_t);

            printf("PUSHV %u", idx);
        }
        break;

        case OPCODE_PUSHA:
        {
            uint32_t cap;
            memcpy(&cap, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            printf("PUSHA %u", cap);
        }
        break;

        case OPCODE_PUSHM:
        {
            uint32_t cap;
            memcpy(&cap, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

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
            memcpy(&idx, p, sizeof(uint8_t));
            p += sizeof(uint8_t);

            printf("SETV %u", idx);
        }
        break;

        case OPCODE_JUMP:
        {
            uint32_t off;
            memcpy(&off, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            printf("JUMP %u", off);
        }
        break;

        case OPCODE_JIFP:
        {
            uint32_t off;
            memcpy(&off, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            printf("JIFP %u", off);
        }
        break;

        case OPCODE_CALL:
        {
            uint32_t off;
            memcpy(&off, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            printf("CALL %u", off);
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

        case OPCODE_SYSVAR:
        {
            uint32_t off;
            memcpy(&off, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            uint32_t len;
            memcpy(&len, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            printf("SYSVAR \"%.*s\"", (int) len, (char*) data + off);
        }
        break;

        case OPCODE_SYSCALL:
        {
            uint32_t off;
            memcpy(&off, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            uint32_t len;
            memcpy(&len, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            printf("SYSCALL \"%.*s\"", (int) len, (char*) data + off);
        }
        break;

        case OPCODE_PUSHT:
        printf("PUSHT");
        break;

        case OPCODE_PUSHFL:
        printf("PUSHFL");
        break;

        case OPCODE_FOR:
        {
            uint8_t a;
            memcpy(&a, p, sizeof(uint8_t));
            p += sizeof(uint8_t);

            uint8_t b;
            memcpy(&b, p, sizeof(uint8_t));
            p += sizeof(uint8_t);

            uint8_t c;
            memcpy(&c, p, sizeof(uint8_t));
            p += sizeof(uint8_t);

            uint32_t d;
            memcpy(&d, p, sizeof(uint32_t));
            p += sizeof(uint32_t);

            printf("FOR %u %u %u %u", a, b, c, d);
        }
        break;
    }

    return p;
}

////////////////////////////////////////////////////////////////////////////////////////
// src/value.h
////////////////////////////////////////////////////////////////////////////////////////

#ifndef WL_VALUE_INCLUDED
#define WL_VALUE_INCLUDED

#ifndef WL_AMALGAMATION
#include "basic.h"
#include "includes.h"
#endif

#define VALUE_NONE  ((Value) 0)
#define VALUE_TRUE  ((Value) 1)
#define VALUE_FALSE ((Value) 2)
#define VALUE_ERROR ((Value) 6)

typedef enum {
    TYPE_NONE,
    TYPE_BOOL,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_MAP,
    TYPE_ARRAY,
    TYPE_STRING,
    TYPE_ERROR,
} Type;

typedef uint64_t Value;

Type    type_of      (Value v);
int64_t get_int      (Value v);
float   get_float    (Value v);
String  get_str      (Value v);
Value   make_int     (WL_Arena *a, int64_t x);
Value   make_float   (WL_Arena *a, float x);
Value   make_str     (WL_Arena *a, String x);
Value   make_map     (WL_Arena *a);
Value   make_array   (WL_Arena *a);
int     map_select   (Value map, Value key, Value *val);
Value*  map_select_by_index(Value map, int key);
int     map_insert   (WL_Arena *a, Value map, Value key, Value val);
Value*  array_select (Value array, int key);
int     array_append (WL_Arena *a, Value array, Value val);
bool    valeq        (Value a, Value b);
bool    valgrt       (Value a, Value b);
int     value_length (Value v);
int     value_to_string(Value v, char *dst, int max);

#endif // WL_VALUE_INCLUDED

////////////////////////////////////////////////////////////////////////////////////////
// src/value.c
////////////////////////////////////////////////////////////////////////////////////////


#ifndef WL_AMALGAMATION
#include "value.h"
#endif

#define ITEMS_PER_MAP_BATCH 8
#define ITEMS_PER_ARRAY_BATCH 16

typedef struct MapItems MapItems;
struct MapItems {
    MapItems *next;
    Value     keys [ITEMS_PER_MAP_BATCH];
    Value     items[ITEMS_PER_MAP_BATCH];
};

typedef struct {
    Type      type;
    int       count;
    int       tail_count;
    MapItems  head;
    MapItems *tail;
} MapValue;

typedef struct ArrayItems ArrayItems;
struct ArrayItems {
    ArrayItems *next;
    Value       items[ITEMS_PER_ARRAY_BATCH];
};

typedef struct {
    Type        type;
    int         count;
    int         tail_count;
    ArrayItems  head;
    ArrayItems *tail;
} ArrayValue;

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

Type type_of(Value v)
{
    // 000 none
    // 001 true
    // 010 false
    // 011 int
    // 100
    // 101
    // 110 error
    // 111 pointer

    switch (v & 7) {
        case 0: return TYPE_NONE;
        case 1: return TYPE_BOOL;
        case 2: return TYPE_BOOL;
        case 3: return TYPE_INT;
        case 4: break;
        case 5: break;
        case 6: return TYPE_ERROR;
        case 7: return *(Type*) ((uintptr_t) v & ~(uintptr_t) 7);
    }

    return TYPE_ERROR;
}

int64_t get_int(Value v)
{
    if ((v & 7) == 3)
        return (int64_t) (v >> 3);

    IntValue *p = (IntValue*) v;
    return p->raw;
}

float get_float(Value v)
{
    FloatValue *p = (FloatValue*) v;
    return p->raw;
}

String get_str(Value v)
{
    StringValue *p = (StringValue*) (v & ~(uintptr_t) 7);
    return (String) { p->data, p->len };
}

static MapValue *get_map(Value v)
{
    return (MapValue*) (v & ~(uintptr_t) 7);
}

static ArrayValue *get_array(Value v)
{
    return (ArrayValue*) (v & ~(uintptr_t) 7);
}

Value make_int(WL_Arena *a, int64_t x)
{
    if (x <= (int64_t) (1ULL << 60)-1 && x >= (int64_t) -(1ULL << 60))
        return ((Value) x << 3) | 3;

    IntValue *v = alloc(a, (int) sizeof(IntValue), _Alignof(IntValue));
    if (v == NULL)
        return VALUE_ERROR;

    v->type = TYPE_INT;
    v->raw  = x;

    assert(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
}

Value make_float(WL_Arena *a, float x)
{
    FloatValue *v = alloc(a, (int) sizeof(FloatValue), _Alignof(FloatValue));
    if (v == NULL)
        return VALUE_ERROR;

    v->type = TYPE_FLOAT;
    v->raw  = x;

    assert(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
}

Value make_str(WL_Arena *a, String x) // TODO: This should reuse the string contents when possible
{
    StringValue *v = alloc(a, (int) sizeof(StringValue) + x.len, 8);
    if (v == NULL)
        return VALUE_ERROR;

    v->type = TYPE_STRING;
    v->len = x.len;
    memcpy(v->data, x.ptr, x.len);

    assert(((uintptr_t) v & 7) == 0);
    return ((Value) v) | 7;
}

Value make_map(WL_Arena *a)
{
    MapValue *m = alloc(a, (int) sizeof(MapValue), _Alignof(MapValue));
    if (m == NULL)
        return VALUE_ERROR;

    m->type = TYPE_MAP;
    m->count = 0;
    m->tail_count = 0;
    m->tail = &m->head;
    m->head.next = NULL;

    return (Value) m | 7;
}

Value make_array(WL_Arena *a)
{
    ArrayValue *v = alloc(a, (int) sizeof(ArrayValue), _Alignof(ArrayValue));
    if (v == NULL)
        return VALUE_ERROR;

    v->type = TYPE_ARRAY;
    v->count = 0;
    v->tail_count = 0;
    v->tail = &v->head;
    v->head.next = NULL;

    return (Value) v | 7;
}

int map_select(Value map, Value key, Value *val)
{
    MapValue *p = get_map(map);
    MapItems *batch = &p->head;
    while (batch) {

        int num = ITEMS_PER_MAP_BATCH;
        if (batch->next == NULL)
            num = p->tail_count;

        for (int i = 0; i < num; i++)
            if (valeq(batch->keys[i], key)) {
                *val = batch->items[i];
                return 0;
            }

        batch = batch->next;
    }

    return -1;
}

Value *map_select_by_index(Value map, int key)
{
    MapValue *p = get_map(map);
    MapItems *batch = &p->head;
    int cursor = 0;
    while (batch) {

        int num = ITEMS_PER_MAP_BATCH;
        if (batch->next == NULL)
            num = p->tail_count;

        if (cursor <= key && key < cursor + num)
            return &batch->keys[key - cursor];

        batch = batch->next;
        cursor += num;
    }

    return NULL;
}

int map_insert(WL_Arena *a, Value map, Value key, Value val)
{
    MapValue *p = get_map(map);
    if (p->tail_count == ITEMS_PER_MAP_BATCH) {

        MapItems *batch = alloc(a, (int) sizeof(MapItems), _Alignof(MapItems));
        if (batch == NULL)
            return -1;

        batch->next = NULL;
        if (p->tail)
            p->tail->next = batch;
        p->tail = batch;
        p->tail_count = 0;
    }

    p->tail->keys[p->tail_count] = key;
    p->tail->items[p->tail_count] = val;
    p->tail_count++;
    p->count++;
    return 0;
}

Value *array_select(Value array, int key)
{
    ArrayValue *p = get_array(array);
    ArrayItems *batch = &p->head;
    int cursor = 0;
    while (batch) {

        int num = ITEMS_PER_ARRAY_BATCH;
        if (batch->next == NULL)
            num = p->tail_count;

        if (cursor <= key && key < cursor + num)
            return &batch->items[key - cursor];

        batch = batch->next;
        cursor += num;
    }

    return NULL;
}

int array_append(WL_Arena *a, Value array, Value val)
{
    ArrayValue *p = get_array(array);
    if (p->tail_count == ITEMS_PER_ARRAY_BATCH) {

        ArrayItems *batch = alloc(a, (int) sizeof(ArrayItems), _Alignof(ArrayItems));
        if (batch == NULL)
            return -1;

        batch->next = NULL;

        if (p->tail)
            p->tail->next = batch;
        p->tail = batch;
        p->tail_count = 0;
    }

    p->tail->items[p->tail_count] = val;
    p->tail_count++;
    p->count++;
    return 0;
}

bool valeq(Value a, Value b)
{
    Type t1 = type_of(a);
    Type t2 = type_of(b);

    if (t1 != t2)
        return false;

    switch (t1) {

        case TYPE_NONE:
        return VALUE_TRUE;

        case TYPE_BOOL:
        return a == b;

        case TYPE_INT:
        return get_int(a) == get_int(b);

        case TYPE_FLOAT:
        return get_float(a) == get_float(b);

        case TYPE_MAP:
        return false; // TODO

        case TYPE_ARRAY:
        return false; // TODO

        case TYPE_STRING:
        return streq(get_str(a), get_str(b));

        case TYPE_ERROR:
        return true;
    }

    return false;
}

bool valgrt(Value a, Value b)
{
    Type t1 = type_of(a);
    Type t2 = type_of(b);

    if (t1 != t2)
        return false;

    switch (t1) {

        case TYPE_NONE:
        return VALUE_FALSE;

        case TYPE_BOOL:
        return VALUE_FALSE;

        case TYPE_INT:
        return get_int(a) > get_int(b);

        case TYPE_FLOAT:
        return get_float(a) > get_float(b);

        case TYPE_MAP:
        return false;

        case TYPE_ARRAY:
        return false;

        case TYPE_STRING:
        return false;

        case TYPE_ERROR:
        return false;
    }

    return false;
}

int value_length(Value v)
{
    Type type = type_of(v);

    if (type == TYPE_ARRAY)
        return get_array(v)->count;

    if (type == TYPE_MAP)
        return get_map(v)->count;

    return -1;
}

typedef struct {
    char *dst;
    int   max;
    int   len;
} ToStringContext;

static void tostr_appends(ToStringContext *tostr, String x)
{
    if (tostr->max > tostr->len) {
        int cpy = tostr->max - tostr->len;
        if (cpy > x.len)
            cpy = x.len;
        memcpy(tostr->dst + tostr->len, x.ptr, cpy);
    }
    tostr->len += x.len;
}

static void tostr_appendi(ToStringContext *tostr, int64_t x)
{
    int len;
    if (tostr->max >= tostr->len)
        len = snprintf(tostr->dst + tostr->len, tostr->max - tostr->len, "%" LLD, x);
    else
        len = snprintf(NULL, 0, "%" LLD, x);
    tostr->len += len;
}

static void tostr_appendf(ToStringContext *tostr, double x)
{
    int len;
    if (tostr->max >= tostr->len)
        len = snprintf(tostr->dst + tostr->len, tostr->max - tostr->len, "%f", x);
    else
        len = snprintf(NULL, 0, "%f", x);
    tostr->len += len;
}

static void value_to_string_inner(Value v, ToStringContext *tostr)
{
    switch (type_of(v)) {

        case TYPE_NONE:
        //tostr_appends(tostr, S("none"));
        break;

        case TYPE_BOOL:
        // TODO
        //tostr_appends(tostr, get_bool(v) ? S("true") : S("false"));
        break;

        case TYPE_INT:
        tostr_appendi(tostr, get_int(v));
        break;

        case TYPE_FLOAT:
        tostr_appendf(tostr, get_float(v));
        break;

        case TYPE_MAP:
        {
            tostr_appends(tostr, S("{ "));
            MapValue *m = get_map(v);
            MapItems *batch = &m->head;
            while (batch) {

                int num = ITEMS_PER_MAP_BATCH;
                if (batch->next == NULL)
                    num = m->tail_count;

                for (int i = 0; i < num; i++) {
                    value_to_string_inner(batch->keys[i], tostr);
                    tostr_appends(tostr, S(": "));
                    value_to_string_inner(batch->items[i], tostr);
                    
                    if (batch->next != NULL || i+1 < num)
                        tostr_appends(tostr, S(", "));
                }

                batch = batch->next;
            }
            tostr_appends(tostr, S(" }"));
        }
        break;

        case TYPE_ARRAY:
        {
            ArrayValue *a = get_array(v);
            ArrayItems *batch = &a->head;
            int cursor = 0;
            while (batch) {

                int num = ITEMS_PER_ARRAY_BATCH;
                if (batch->next == NULL)
                    num = a->tail_count;

                for (int i = 0; i < num; i++)
                    value_to_string_inner(batch->items[i], tostr);

                batch = batch->next;
                cursor += num;
            }
        }
        break;

        case TYPE_STRING:
        tostr_appends(tostr, get_str(v));
        break;

        case TYPE_ERROR:
        tostr_appends(tostr, S("error"));
        break;
    }
}

int value_to_string(Value v, char *dst, int max)
{
    ToStringContext tostr = { dst, max, 0 };
    value_to_string_inner(v, &tostr);
    return tostr.len;
}

////////////////////////////////////////////////////////////////////////////////////////
// src/eval.h
////////////////////////////////////////////////////////////////////////////////////////

#ifndef WL_EVAL_INCLUDED
#define WL_EVAL_INCLUDED

#ifndef WL_AMALGAMATION
#include "assemble.h"
#endif

// TODO: pretty sure this is unused
int eval(WL_Program p, WL_Arena *a, char *errbuf, int errmax);

#endif // WL_EVAL_INCLUDED

////////////////////////////////////////////////////////////////////////////////////////
// src/eval.c
////////////////////////////////////////////////////////////////////////////////////////


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

            printf("for end %u\n", end); // TODO

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

////////////////////////////////////////////////////////////////////////////////////////
// src/compile.c
////////////////////////////////////////////////////////////////////////////////////////


#ifndef WL_AMALGAMATION
#include "eval.h"
#include "parse.h"
#include "assemble.h"
#include "compile.h"
#endif

#define FILE_LIMIT 32

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
};

int WL_streq(WL_String a, char *b, int blen)
{
    if (b == NULL) b = "";
    if (blen < 0) blen = strlen(b);

    if (a.len != blen)
        return 0;

    for (int i = 0; i < a.len; i++)
        if (a.ptr[i] != b[i])
            return 0;

    return 1;
}

WL_Compiler *WL_Compiler_init(WL_Arena *arena)
{
    WL_Compiler *compiler = alloc(arena, (int) sizeof(WL_Compiler), _Alignof(WL_Compiler));
    if (compiler == NULL)
        return NULL;
    compiler->arena = arena;
    compiler->num_files = 0;
    compiler->waiting_file = (String) { NULL, 0 };
    return compiler;
}

void WL_Compiler_free(WL_Compiler *compiler)
{
    (void) compiler;
    // TODO
}

WL_CompileResult WL_compile(WL_Compiler *compiler, WL_String file, WL_String content)
{
    if (compiler->waiting_file.len > 0)
        file = (WL_String) { compiler->waiting_file.ptr, compiler->waiting_file.len };
    else {
        // TODO: copy file path
        // file = strdup(file, compiler->arena)
    }

    char err[1<<9];
    ParseResult pres = parse((String) { content.ptr, content.len }, compiler->arena, err, (int) sizeof(err));
    if (pres.node == NULL) {
        printf("%s\n", err); // TODO
        return (WL_CompileResult) { .type=WL_COMPILE_RESULT_ERROR };
    }

    CompiledFile compiled_file = {
        .file = { file.ptr, file.len },
        .root = pres.node,
        .includes = pres.includes,
    };
    compiler->files[compiler->num_files++] = compiled_file;

    for (int i = 0; i < compiler->num_files; i++) {

        Node *include = compiler->files[i].includes;
        while (include) {

            assert(include->type == NODE_INCLUDE);

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
                    assert(0); // TODO
                }

                // TODO: Make the path relative to the compiled file

                compiler->waiting_file = include->include_path;
                return (WL_CompileResult) { .type=WL_COMPILE_RESULT_FILE, .path={ include->include_path.ptr, include->include_path.len } };
            }

            include = include->include_next;
        }
    }

    AssembleResult ares = assemble(compiler->files[0].root, compiler->arena, err, (int) sizeof(err));
    if (ares.errlen) {
        printf("%s\n", err); // TODO
        return (WL_CompileResult) { .type=WL_COMPILE_RESULT_ERROR };
    }

    return (WL_CompileResult) { .type=WL_COMPILE_RESULT_DONE, .program=ares.program };
}

void WL_dump_program(WL_Program program)
{
    print_program(program);
}
