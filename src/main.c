#include <stdio.h>
#include <stdlib.h>

#include "file.h"
#include "parse.h"

int main(void)
{
    String src;
    int ret = file_read_all(S("main.wl"), &src);
    if (ret < 0) {
        printf("Error!\n");
        return -1;
    }

    char err[1<<9];
    char mem[1<<14];
    Arena a = { mem, COUNT(mem), 0 };

    ParseResult result = parse(src, &a, err, COUNT(err));
    if (result.node == NULL) {
        printf("%.*s\n", result.errlen, err);
        return -1;
    }

    print_node(result.node);
    fflush(stdout);

    free(src.ptr);
    return 0;
}
