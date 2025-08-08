
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
