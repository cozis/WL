#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "wl.h"

typedef struct FileData FileData;

struct FileData {
    FileData *next;
    int       size;
    char      data[];
};

FileData *load_file(WL_String path)
{
    char buf[1<<10];
    if (path.len >= (int) sizeof(buf))
        return NULL;
    memcpy(buf, path.ptr, path.len);
    buf[path.len] = '\0';

    FILE *f = fopen(buf, "rb");
    if (f == NULL)
        return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    FileData *data = malloc(sizeof(FileData) + size);
    if (data == NULL) {
        fclose(f);
        return NULL;
    }
    data->size = size;
    data->next = NULL;

    fread(data->data, 1, size, f);
    fclose(f);
    return data;
}

int main(int argc, char **argv)
{
    char *entry_file = NULL;

    bool bc = false;
    bool ast = false;
    bool run = true;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--bc"))
            bc = true;
        else if (!strcmp(argv[i], "--ast"))
            ast = true;
        else if (!strcmp(argv[i], "--no-run"))
            run = false;
        else
            entry_file = argv[i];
    }

    if (entry_file == NULL) {
        fprintf(stderr, "Usage: %s file.wl\n", argv[0]);
        return -1;
    }

    int cap = 1<<20;
    char *mem = malloc(cap);
    if (mem == NULL) {
        fprintf(stderr, "Error: Allocation failure\n");
        return -1;
    }
    WL_Arena arena = { mem, cap, 0 };

    WL_Program program;
    {
        WL_Compiler *c = wl_compiler_init(&arena);
        if (c == NULL) {
            fprintf(stderr, "Error: Out of memory");
            return -1;
        }

        FileData *file_head;
        FileData **file_tail = &file_head;

        WL_String path = { entry_file, strlen(entry_file) };
        for (;;) {

            FileData *file = load_file(path);
            if (file == NULL) {
                printf("Couldn't open '%.*s'\n", path.len, path.ptr);
                return -1;
            }
            *file_tail = file;
            file_tail = &file->next;

            WL_AddResult res = wl_compiler_add(c, (WL_String) { file->data, file->size });
            if (res.type == WL_ADD_ERROR) {
                fprintf(stderr, "Error: %s\n", wl_compiler_error(c).ptr);
                return -1;
            }
            if (res.type == WL_ADD_AGAIN) {
                path = res.path;
                continue;
            }
            assert(res.type == WL_ADD_LINK);
            break;
        }

        *file_tail = NULL;

        if (ast) {
            char buf[1<<10];
            int len = wl_dump_ast(c, buf, sizeof(buf));
            if (len > sizeof(buf)-1)
                len = sizeof(buf)-1;
            buf[len] = '\0';

            printf("%s\n", buf);
        }

        int ret = wl_compiler_link(c, &program);
        if (ret < 0) {
            WL_String err = wl_compiler_error(c);
            fprintf(stderr, "Error: %s\n", err.ptr);
            return -1;
        }

        if (bc)
            wl_dump_program(program);

        FileData *file = file_head;
        while (file) {
            FileData *next = file->next;
            free(file);
            file = next;
        }
    }

    if (run) {
        WL_Runtime *rt = wl_runtime_init(&arena, program);
        if (rt == NULL) {
            fprintf(stderr, "Error: Invalid program or out of memory\n");
            return -1;
        }

        FILE *output = stdout;
        for (bool done = false; !done; ) {
            WL_EvalResult res = wl_runtime_eval(rt);

            wl_runtime_dump(rt);

            switch (res.type) {

                case WL_EVAL_NONE:
                break;

                case WL_EVAL_DONE:
                done = true;
                break;

                case WL_EVAL_ERROR:
                printf("Error: %s\n", wl_runtime_error(rt).ptr);
                return -1;

                case WL_EVAL_OUTPUT:
                fwrite(res.str.ptr, 1, res.str.len, output);
                break;

                case WL_EVAL_SYSVAR:
                if (wl_streq(res.str, "varA", -1)) wl_push_s64(rt, 1);
                if (wl_streq(res.str, "varB", -1)) wl_push_s64(rt, 7);
                if (wl_streq(res.str, "varC", -1)) wl_push_s64(rt, 13);
                break;

                case WL_EVAL_SYSCALL:
                if (wl_streq(res.str, "testfn", -1)) {
                    for (int i = 0; i < wl_arg_count(rt); i++)
                        wl_push_arg(rt, i);
                }
                break;
            }
        }
    }

    return 0;
}