#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "WL.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Missing file path\n");
        return -1;
    }
    char *file = argv[1];

    char err[1<<9];
    char *mem = malloc(1<<20);
    WL_Arena a = { mem, 1<<20, 0 };

    WL_Compiler *compiler = WL_Compiler_init(&a);
    if (compiler == NULL) {
        assert(0); // TODO
    }

    int num_loaded_files = 0;
    char *loaded_files[128];

    WL_CompileResult result;
    WL_String path = { file, strlen(file) };
    for (int i = 0;; i++) {

        char buf[1<<10];
        if (path.len >= (int) sizeof(buf)) {
            assert(0); // TODO
        }
        memcpy(buf, path.ptr, path.len);
        buf[path.len] = '\0';

        FILE *f = fopen(buf, "rb");
        if (f == NULL) {
            printf("File not found '%.*s'\n", path.len, path.ptr);
            return -1;
        }

        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        char *file_data = malloc(file_size);

        fread(file_data, 1, file_size, f);
        fclose(f);

        result = WL_compile(compiler, path, (WL_String) { file_data, file_size });

        loaded_files[num_loaded_files++] = file_data;

        if (result.type == WL_COMPILE_RESULT_ERROR) {
            printf("Compilation of '%.*s' failed\n", path.len, path.ptr);
            break;
        }

        if (result.type == WL_COMPILE_RESULT_DONE)
            break;

        assert(result.type == WL_COMPILE_RESULT_FILE);
        path = result.path;
    }

    for (int i = 0; i < num_loaded_files; i++)
        free(loaded_files[i]);

    WL_Compiler_free(compiler);

    if (result.type == WL_COMPILE_RESULT_ERROR) {
        printf("Compilation error\n");
        return -1;
    }
    WL_Program program = result.program;

    WL_State *state = WL_State_init(&a, program, err, (int) sizeof(err));

    WL_State_trace(state, 0);

    for (bool done = false; !done; ) {

        WL_Result result = WL_eval(state);
        switch (result.type) {

            case WL_DONE:
            done = true;
            break;

            case WL_ERROR:
            done = true;
            printf("%s\n", err);
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
    free(mem);
    return 0;
}
