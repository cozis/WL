#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "WL.h"

int main(void)
{
    FILE *f = fopen("main.wl", "rb");
    if (f == NULL)
        return -1;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *file_data = malloc(file_size);

    fread(file_data, 1, file_size, f);
    fclose(f);

    char err[1<<9];
    char *mem = malloc(1<<20);
    WL_Arena a = { mem, 1<<20, 0 };

    WL_Program program;
    int ret = WL_compile(file_data, file_size, a, &program, err, (int) sizeof(err));
    if (ret < 0) {
        printf("%s\n", err);
        return -1;
    }

    WL_State *state = WL_State_init(&a, program, err, (int) sizeof(err));

    for (bool done = false; !done; ) {

        WL_Result result = WL_eval(state);
        switch (result.type) {

            case WL_DONE:
            done = true;
            break;

            case WL_ERROR:
            done = true;
            break;

            case WL_VAR:
            if (0) {}
            else if (WL_streq(result.str, "varA", -1)) WL_pushint(state, 1);
            else if (WL_streq(result.str, "varB", -1)) WL_pushint(state, 1);
            else if (WL_streq(result.str, "varC", -1)) WL_pushint(state, 1);
            break;

            case WL_CALL:
            // TODO
            printf("(called [%.*s])\n", result.str.len, result.str.ptr);
            break;

            case WL_OUTPUT:
            fwrite(result.str.ptr, 1, result.str.len, stdout);
            break;
        }
    }

    WL_State_free(state);
    free(file_data);
    free(mem);
    return 0;
}
