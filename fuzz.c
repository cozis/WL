#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "wl.h"

#define MAX_INPUT_SIZE (64 * 1024)  // 64KB max input
#define ARENA_SIZE (1 << 20)         // 1MB arena

// Persistent mode: AFL will call this loop many times without restarting
__AFL_FUZZ_INIT();

int main(int argc, char **argv)
{
    // Allocate arena once (outside the AFL loop)
    char *mem = malloc(ARENA_SIZE);
    if (mem == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        return -1;
    }

    // Buffer for input
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    // AFL persistent mode: loop up to 1000 times per process
    while (__AFL_LOOP(1000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        
        // Limit input size
        if (len > MAX_INPUT_SIZE) {
            len = MAX_INPUT_SIZE;
        }
        
        // Reset arena for each iteration
        WL_Arena arena = { mem, ARENA_SIZE, 0 };
        
        // Initialize compiler
        WL_Compiler *c = wl_compiler_init(&arena);
        if (c == NULL) {
            continue;  // Out of memory, skip this input
        }
        
        // Try to compile the input
        WL_AddResult res = wl_compiler_add(
            c, 
            (WL_String) { "", 0 },  // empty path
            (WL_String) { (char*)buf, len }
        );
        
        // If compilation succeeded, try to link
        if (res.type == WL_ADD_LINK) {
            WL_Program program;
            int ret = wl_compiler_link(c, &program);
            
            // If linking succeeded, optionally execute (for deeper fuzzing)
            if (ret == 0) {
                WL_Runtime *rt = wl_runtime_init(&arena, program);
                if (rt != NULL) {
                    // Execute up to 1000 steps to avoid infinite loops
                    for (int i = 0; i < 1000; i++) {
                        WL_EvalResult eval_res = wl_runtime_eval(rt);
                        
                        if (eval_res.type == WL_EVAL_DONE || 
                            eval_res.type == WL_EVAL_ERROR) {
                            break;
                        }
                        
                        // Handle syscalls/sysvars by pushing dummy values
                        if (eval_res.type == WL_EVAL_SYSVAR) {
                            wl_push_s64(rt, 42);
                        } else if (eval_res.type == WL_EVAL_SYSCALL) {
                            wl_push_s64(rt, 42);
                        }
                    }
                }
            }
        }
        
        // Arena is automatically reset on next iteration
    }

    free(mem);
    return 0;
}