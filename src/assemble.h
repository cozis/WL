#ifndef WL_ASSEMBLE_INCLUDED
#define WL_ASSEMBLE_INCLUDED

#ifndef WL_AMALGAMATION
#include "public.h"
#include "parse.h"
#endif

enum {
    OPCODE_NOPE    = 0x00,
    OPCODE_EXIT    = 0x23,
    OPCODE_GROUP   = 0x25,
    OPCODE_GPOP    = 0x26,
    OPCODE_GPRINT  = 0x27,
    OPCODE_GTRUNC  = 0x28,
    OPCODE_GCOALESCE = 0x29,
    OPCODE_GOVERWRITE = 0x2A,
    OPCODE_GPACK   = 0x2B,
    OPCODE_PUSHI   = 0x01,
    OPCODE_PUSHF   = 0x02,
    OPCODE_PUSHS   = 0x03,
    OPCODE_PUSHV   = 0x04,
    OPCODE_PUSHA   = 0x05,
    OPCODE_PUSHM   = 0x06,
    OPCODE_PUSHN   = 0x21,
    OPCODE_POP     = 0x07,
    OPCODE_NEG     = 0x08,
    OPCODE_EQL     = 0x09,
    OPCODE_NQL     = 0x0A,
    OPCODE_LSS     = 0x0B,
    OPCODE_GRT     = 0x0C,
    OPCODE_ADD     = 0x0D,
    OPCODE_SUB     = 0x0E,
    OPCODE_MUL     = 0x0F,
    OPCODE_DIV     = 0x10,
    OPCODE_MOD     = 0x11,
    OPCODE_SETV    = 0x12,
    OPCODE_JUMP    = 0x13,
    OPCODE_JIFP    = 0x14,
    OPCODE_CALL    = 0x15,
    OPCODE_RET     = 0x16,
    OPCODE_APPEND  = 0x17,
    OPCODE_INSERT1 = 0x18,
    OPCODE_INSERT2 = 0x19,
    OPCODE_SELECT  = 0x20,
    OPCODE_PRINT   = 0x24,
    OPCODE_SYSVAR  = 0x2C,
    OPCODE_SYSCALL = 0x2D,
    OPCODE_FOR     = 0x2E,
};

typedef struct {
    WL_Program program;
    int errlen;
} AssembleResult;

int parse_program_header(WL_Program p, String *code, String *data, char *errbuf, int errmax);
void  print_program(WL_Program program);
char *print_instruction(char *p, char *data);
AssembleResult assemble(Node *root, WL_Arena *arena, char *errbuf, int errmax);

#endif // WL_ASSEMBLE_INCLUDED