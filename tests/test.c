#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "../wl.h"

#define COUNT(X) (int) (sizeof(X) / sizeof((X)[0]))

struct {
    int line;
    char *in;
    char *out;
} tests[] = {
    {__LINE__, "", ""},
    {__LINE__, "1", "1"},
    {__LINE__, "none", ""},
    {__LINE__, "true", "true"},
    {__LINE__, "false", "false"},
    {__LINE__, "\"Hello, world!\"", "Hello, world!"},
    {__LINE__, "'Hello, world!'", "Hello, world!"},
    {__LINE__, "\"\\\\\"", "\\"},
    {__LINE__, "\"\\n\"", "\n"},
    {__LINE__, "\"\\t\"", "\t"},
    {__LINE__, "\"\\r\"", "\r"},
    {__LINE__, "\"\\\"\"", "\""},
    {__LINE__, "\"\\'\"", "'"},
    {__LINE__, "\"\\xFF\"", "\xFF"},
    {__LINE__, "\"\\x0F\"", "\x0F"},
    {__LINE__, "\"\\xF0\"", "\xF0"},
    {__LINE__, "[1, 2, 3]", "123"},
    {__LINE__, "+{}", "{}"},
    {__LINE__, "+{a:1}", "{a: 1}"},
    {__LINE__, "+{a:1,b:2,c:3}", "{a: 1, b: 2, c: 3}"},
    {__LINE__, "10-1-2", "7"},
    {__LINE__, "1+2*3", "7"},
    {__LINE__, "(1+2)*3", "9"},
    {__LINE__, "1+(2*3)", "7"},
    {__LINE__, "1+2", "3"},
    {__LINE__, "2-1", "1"},
    {__LINE__, "1-2", "-1"},
    {__LINE__, "2*3", "6"},
    {__LINE__, "2/3", "0"},
    {__LINE__, "4/2", "2"},
    {__LINE__, "0%2", "0"},
    {__LINE__, "3%2", "1"},
    {__LINE__, "1.0+2.0", "3.00"},
    {__LINE__, "2.0-1.0", "1.00"},
    {__LINE__, "1.0-2.0", "-1.00"},
    {__LINE__, "2.0*3.0", "6.00"},
    {__LINE__, "2.0/3.0", "0.67"},
    {__LINE__, "4.0/2.0", "2.00"},
    {__LINE__, "1+2.0", "3.00"},
    {__LINE__, "2-1.0", "1.00"},
    {__LINE__, "1-2.0", "-1.00"},
    {__LINE__, "2*3.0", "6.00"},
    {__LINE__, "2/3.0", "0.67"},
    {__LINE__, "4/2.0", "2.00"},
    {__LINE__, "1.0+2", "3.00"},
    {__LINE__, "2.0-1", "1.00"},
    {__LINE__, "1.0-2", "-1.00"},
    {__LINE__, "2.0*3", "6.00"},
    {__LINE__, "2.0/3", "0.67"},
    {__LINE__, "4.0/2", "2.00"},
    {__LINE__, "len []", "0"},
    {__LINE__, "len [1, 2, 3]", "3"},
    {__LINE__, "len {}", "0"},
    {__LINE__, "len {a:1}", "1"},
    {__LINE__, "len {a:1,b:2,c:3}", "3"},
    {__LINE__, "-12", "-12"},
    {__LINE__, "-12.34", "-12.34"},
    {__LINE__, "1<2", "true"},
    {__LINE__, "2<1", "false"},
    {__LINE__, "1>2", "false"},
    {__LINE__, "2>1", "true"},
    {__LINE__, "1==1", "true"},
    {__LINE__, "1==2", "false"},
    {__LINE__, "1!=1", "false"},
    {__LINE__, "1!=2", "true"},
    {__LINE__, "let a = 1\na", "1"},
    {__LINE__, "let a = 1\na = 2\na", "2"},
    {__LINE__, "[5, 6, 7][0]", "5"},
    {__LINE__, "[5, 6, 7][1]", "6"},
    {__LINE__, "[5, 6, 7][2]", "7"},
    {__LINE__, "+{a:5,b:6,c:7}.a", "5"},
    {__LINE__, "+{a:5,b:6,c:7}.b", "6"},
    {__LINE__, "+{a:5,b:6,c:7}.c", "7"},
    {__LINE__, "+{a:5,b:6,c:7}['a']", "5"},
    {__LINE__, "+{a:5,b:6,c:7}['b']", "6"},
    {__LINE__, "+{a:5,b:6,c:7}['c']", "7"},
    {__LINE__, "let x = 1\n{\nlet x = 2\nx\n}", "2"},
    {__LINE__, "let x = {a:5,b:6,c:7}\nx.a = true\nx", "{a: true, b: 6, c: 7}"},
    {__LINE__, "let x = {a:5,b:6,c:7}\nx.b = true\nx", "{a: 5, b: true, c: 7}"},
    {__LINE__, "let x = {a:5,b:6,c:7}\nx.c = true\nx", "{a: 5, b: 6, c: true}"},
    {__LINE__, "let x = {a:5,b:6,c:7}\nx['a'] = true\nx", "{a: true, b: 6, c: 7}"},
    {__LINE__, "let x = {a:5,b:6,c:7}\nx['b'] = true\nx", "{a: 5, b: true, c: 7}"},
    {__LINE__, "let x = {a:5,b:6,c:7}\nx['c'] = true\nx", "{a: 5, b: 6, c: true}"},
    {__LINE__, "let a\nlet b\nlet c\na = b = c = 5\n", ""},
    {__LINE__, "let a\nlet b\nlet c\na = b = c = 5\na\nb\nc", "555"},
    {__LINE__, "if 1 < 2: 1\n2", "12"},
    {__LINE__, "if 1 > 2: 1\n2", "2"},
    {__LINE__, "if 1 < 2: 1 else 2\n3", "13"},
    {__LINE__, "if 1 > 2: 1 else 2\n3", "23"},
    {__LINE__, "let i = 0\nwhile i < 3: {\ntrue\ni = i + 1\n}\n", "truetruetrue"},
    {__LINE__, "for a in ['A', 'B', 'C']: a", "ABC"},
    {__LINE__, "for a, b in ['A', 'B', 'C']: { a\n b }", "A0B1C2"},
    {__LINE__, "for a in {x:1,y:2,z:3}: a", "xyz"},
    {__LINE__, "for a, b in {x:1,y:2,z:3}: { a\n b }", "x0y1z2"},
    {__LINE__, "procedure P() 0", ""},
    {__LINE__, "procedure P(a) a", ""},
    {__LINE__, "procedure P(a, b, c) { a\nb\nc }", ""},
    {__LINE__, "procedure P() 0\nP()", "0"},
    {__LINE__, "procedure P(a, b, c) { a\nb\nc }\nP()", ""},
    {__LINE__, "procedure P(a, b, c) { a\nb\nc }\nP(1)", "1"},
    {__LINE__, "procedure P(a, b, c) { a\nb\nc }\nP(1, 2)", "12"},
    {__LINE__, "procedure P(a, b, c) { a\nb\nc }\nP(1, 2, 3)", "123"},
    {__LINE__, "procedure N(n) n\nN(1) + N(2)", "3"},
    {__LINE__, "P()\nprocedure P() 1", "1"},
    {__LINE__, "P()\nprocedure P() 1\n{\nprocedure P() 2\n}", "1"},
    {__LINE__, "procedure P() 1\nP()\n{\nprocedure P() 2\n}", "1"},
    {__LINE__, "procedure P() 1\n{\nP()\nprocedure P() 2\n}", "2"},
    {__LINE__, "procedure P() 1\n{\nprocedure P() 2\nP()\n}", "2"},
    {__LINE__, "procedure P() 1\n{\nprocedure P() 2\n}\nP()", "1"},
    {__LINE__, "<a>Hello, world!</a>", "<a>Hello, world!</a>"},
    {__LINE__, "<ul><li>A</li><li>B</li><li>C</li></ul>", "<ul><li>A</li><li>B</li><li>C</li></ul>"},
    {__LINE__, "let a = <ul><li>A</li><li>B</li><li>C</li></ul>", ""},
    {__LINE__, "let a = <ul><li>A</li><li>B</li><li>C</li></ul>\na", "<ul><li>A</li><li>B</li><li>C</li></ul>"},
};

int run_test(char *in, char *out, void *mem, int cap, int test_line)
{
    WL_Arena arena = { mem, cap, 0 };
    WL_Program program;

    WL_Compiler *c = wl_compiler_init(&arena);
    if (c == NULL) {
        fprintf(stderr, "Error: Out of memory");
        return -1;
    }

    WL_AddResult res = wl_compiler_add(c, (WL_String) { NULL, 0 }, (WL_String) { in, strlen(in) });
    if (res.type == WL_ADD_ERROR) {
        fprintf(stderr, "Error: %s\n", wl_compiler_error(c).ptr);
        return -1;
    }
    if (res.type != WL_ADD_LINK) {
        fprintf(stderr, "Error: Unexpected compiler state\n");
        return -1;
    }

    int ret = wl_compiler_link(c, &program);
    if (ret < 0) {
        WL_String err = wl_compiler_error(c);
        fprintf(stderr, "Error: %s\n", err.ptr);
        return -1;
    }

    WL_Runtime *rt = wl_runtime_init(&arena, program);
    if (rt == NULL) {
        fprintf(stderr, "Error: Invalid program or out of memory\n");
        return -1;
    }

    char output[1<<10];
    int outlen = 0;

    for (bool done = false; !done; ) {
        WL_EvalResult res = wl_runtime_eval(rt);
        switch (res.type) {

            case WL_EVAL_NONE:
            break;

            case WL_EVAL_DONE:
            done = true;
            break;

            case WL_EVAL_ERROR:
            printf("Error: %s (test at line %d)\n", wl_runtime_error(rt).ptr, test_line);
            return 0;

            case WL_EVAL_OUTPUT:
            if ((int) sizeof(output) - outlen < res.str.len) {
                printf("Error: Output is too long\n");
                return -1;
            }
            memcpy(output + outlen, res.str.ptr, res.str.len);
            outlen += res.str.len;
            break;

            case WL_EVAL_SYSVAR:
            break;

            case WL_EVAL_SYSCALL:
            break;
        }
    }

    //printf("[%s] -> [%.*s]\n", in, outlen, output);

    if (outlen != strlen(out) || memcmp(out, output, outlen)) {
        fprintf(stderr, "Error: Output mistmatch\n");
        fprintf(stderr, "  Source  : [%s]\n", in);
        fprintf(stderr, "  Output  : [%.*s]\n", outlen, output);
        fprintf(stderr, "  Expected: [%s]\n", out);
#if 1
        fprintf(stderr, "  Program:\n");
        wl_dump_program(program);
#endif
        return 0;
    }

    return 1;
}

int main(void)
{
    int cap = 1<<20;
    char *mem = malloc(cap);
    if (mem == NULL)
        return -1;

    for (int i = 0; i < COUNT(tests); i++)
        run_test(tests[i].in, tests[i].out, mem, cap, tests[i].line);

    free(mem);
    return 0;
}
