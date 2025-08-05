
#ifndef WL_AMALGAMATION
#include "eval.h"
#include "parse.h"
#include "assemble.h"
#include "WL.h"
#endif

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

int WL_compile(char *src, int len, WL_Arena arena, WL_Program *program, char *err, int errmax)
{
    if (src == NULL) src = "";
    if (len < 0) len = strlen(src);

    Node *root;
    ParseResult pres = parse((String) {src, len}, &arena, err, errmax);
    if (pres.node == NULL)
        return -1;
    root = pres.node;

    AssembleResult ares = assemble(root, &arena, err, errmax);
    if (ares.errlen)
        return -1;
    *program = ares.program;

    return 0;
}