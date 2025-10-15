# WL
WL is a powerful and flexible, yet experimental scripting language for templating with first-class support for HTML.

To learn about the language check out the `MANUAL.md` file. But for a sneak peek, here's an example:

```
let title = "Title of my webpage"
let items = ["A", "B", "C"]

let navigator =
    <nav>
        <a href="/home">Home</a>
        <a href="/about">About</a>
    </nav>

let some_list = 
    <ul>
    \for item in items:
        <li>\{escape item}</li>
    </ul>

<html>
    <head>
        <title>\{escape title}</title>
    </head>
    <body>
        \{navigator}
        <article>
            \{some_list}
        </article>
    </body>
</html>
```

## Features

1. Turing-complete language with first-class HTML support, procedures, and an import system
2. Easy to embed: The interpreter is a single C file with no dependencies that performs no allocations or I/O
3. Built-in XSS protection: `escape()` operator to sanitize dynamic HTML

## Getting Started

The WL interpreter is intended to be used as a library, but you can use the CLI to get a feel for the language. You can compile it by running:

```
make wl
```

This will generate the `wl` executable that you can call to evaluate `.wl` files.

If you are using vscode, you can also install the language extension `ide/vscode/wl-language` by dropping it into your editor's extension folder and reloading it. The extension folder should be one of these:
* Windows: `%USERPROFILE%\.vscode\extensions`
* macOS: `~/.vscode/extensions`
* Linux: `~/.vscode/extensions`

If you're sold on the language and want to embed it in your application, just add the `wl.c` and `wl.h` files to your build and read the "Embedding" section.

## Embedding

WL programs need to first be translated to bytecode, then evaluated in a virtual machine. The bytecode is completely standalone and can be cached.

The API is quite involved as it tries not to take resource ownership from the caller. Ideally parent applications will have their own simplified wrapper over this API with caching and support for their own object model.

### Compilation

To compile a script, you need to create a `WL_Compiler` object and add the source file to it

```c
#include <stdio.h>
#include "wl.h"

int main(void)
{
    WL_String source = WL_STR("<p>Hello, world!</p>");

    // Allocate some memory for the compiler
    char memory[1<<16];
    WL_Arena arena = { memory, sizeof(memory), 0 };

    // Create the translation unit object
    WL_Compiler *c = wl_compiler_init(&arena);
    if (c == NULL) { /* error */ }

    // Add a file to the unit
    WL_AddResult res = wl_compiler_add(c, (WL_String) { NULL, 0 }, source);

    if (res.type == WL_ADD_ERROR) {
        fprintf(stderr, "Error: %s\n", wl_compiler_error(c).ptr);
        return -1;
    }

    if (res.type != WL_ADD_LINK) {
        fprintf(stderr, "Error: Unexpected compiler state\n");
        return -1;
    }

    // Produce the template executable
    WL_Program program;
    int ret = wl_compiler_link(c, &program);
    if (ret < 0) {
        WL_String err = wl_compiler_error(c);
        fprintf(stderr, "Error: %s\n", err.ptr);
        return -1;
    }

    // Done!
    // The WL_Program is just a string of bytes you can
    // write to a file or store in a cache
    return 0;
}
```

If the initial script includes other files, the `wl_compiler_add` function will return an `WL_AddResult` of type `WL_ADD_AGAIN` and contain the path of the file that needs to be added next. The program will then need to call `wl_compiler_add` again with that file, until either an error occurs or `WL_ADD_LINK` is returned.

### Evaluation

Once a bytecode program has been obtained, this is how you set up the virtual machine to run it:

```c
#include <stdio.h>
#include "wl.h"

int main(void)
{
    WL_Program program;
    get_program_from_somewhere(&program);

    WL_Runtime *rt = wl_runtime_init(&arena, program);
    if (rt == NULL) {
        printf("error\n");
        return -1;
    }

    for (bool done = false; !done; ) {

        WL_EvalResult res = wl_runtime_eval(rt);

        switch (res.type) {

            case WL_EVAL_NONE:
            // Dummy value. This is never returned.
            break;

            case WL_EVAL_DONE:
            // Evaluation complete
            done = true;
            break;

            case WL_EVAL_ERROR:
            // Runtime error occurred
            printf("Error: %s\n", wl_runtime_error(rt).ptr);
            return -1;

            case WL_EVAL_OUTPUT:
            // Output string available
            fwrite(res.str.ptr, 1, res.str.len, stdout);
            break;

            case WL_EVAL_SYSVAR:
            // External variable referenced
            break;

            case WL_EVAL_SYSCALL:
            // External function called
            break;
        }
    }

    return 0;
}
```

### External Symbols

When during the evaluation of a program the `WL_EVAL_SYSVAR` result is returned, it means the program referenced an external symbol as a variable. The host program needs to push onto the stack of the VM the value relative to that symbol.

#### External Variables

Say your environment defined three external symbols "varA", "varB", "varC" with values 1, 2, 3. The way you would implement this is by doing:

```c
for (bool done = false; !done; ) {

    WL_EvalResult res = wl_runtime_eval(rt);

    switch (res.type) {

        case WL_EVAL_NONE:
        break;

        case WL_EVAL_DONE:
        done = true;
        break;

        case WL_EVAL_ERROR:
        return -1;

        case WL_EVAL_OUTPUT:
        fwrite(res.str.ptr, 1, res.str.len, output);
        break;

        case WL_EVAL_SYSVAR:
        
        if (wl_streq(res.str, "varA", -1))
            wl_push_s64(rt, 1);

        if (wl_streq(res.str, "varB", -1))
            wl_push_s64(rt, 2);

        if (wl_streq(res.str, "varC", -1))
            wl_push_s64(rt, 3);

        break;

        case WL_EVAL_SYSCALL:
        // External function called
        break;
    }
}
```

You first check the name of the referenced symbol in `res.str`, then use one of the `wl_push_*` functions to add the associated value.

#### External Calls

If the program performs a call to an external function, the VM will return a result of type `WL_EVAL_SYSCALL`.

The parent program can then get the number of arguments using the `wl_arg_count` function and `wl_push_arg` to set the top of the VM stack to the argument with the specified index. The argument can then be read using one of the `wl_pop_*` functions.

The caller then needs to push the return value of the call on top of the stack using one of the `wl_push_*` functions.
