#include <stdio.h>
#include <stdlib.h>

#include "file.h"
#include "eval.h"
#include "parse.h"
#include "assemble.h"

int main(void)
{
    String src;
    int ret = file_read_all(S("main.wl"), &src);
    if (ret < 0) {
        printf("Error!\n");
        return -1;
    }

    char err[1<<9];
    char *mem = malloc(1<<20);
    Arena a = { mem, 1<<20, 0 };

    ParseResult parse_result = parse(src, &a, err, COUNT(err));
    if (parse_result.node == NULL) {
        printf("%.*s\n", parse_result.errlen, err);
        return -1;
    }

    //print_node(parse_result.node);
    //printf("\n");

    AssembleResult assemble_result = assemble(parse_result.node, &a, err, COUNT(err));
    if (assemble_result.errlen) {
        printf("%.*s\n", parse_result.errlen, err);
        return -1;
    }

    //print_program(assemble_result.program);

    ret = eval(assemble_result.program, &a, err, COUNT(err));
    if (ret < 0) {
        printf("%s\n", err);
        return -1;
    }

    free(src.ptr);
    free(mem);
    return 0;
}
