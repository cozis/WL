
enum {
    OPCODE_NOPE  = 0x00,
    OPCODE_PUSHI = 0x01,
    OPCODE_PUSHF = 0x02,
    OPCODE_PUSHS = 0x03,
    OPCODE_PUSHV = 0x04,
    OPCODE_PUSHA = 0x05,
    OPCODE_PUSHM = 0x06,
    OPCODE_POP   = 0x07,
    OPCODE_NEG   = 0x08,
    OPCODE_EQL   = 0x09,
    OPCODE_NQL   = 0x0A,
    OPCODE_LSS   = 0x0B,
    OPCODE_GRT   = 0x0C,
    OPCODE_ADD   = 0x0D,
    OPCODE_SUB   = 0x0E,
    OPCODE_MUL   = 0x0F,
    OPCODE_DIV   = 0x10,
    OPCODE_MOD   = 0x20,
    OPCODE_JUMP  = 0x30,
    OPCODE_JIFP  = 0x40,
    OPCODE_CALL  = 0x41,
    OPCODE_RET   = 0x42,
};

typedef struct {
    void *ptr;
    int   len;
} Program;

typedef struct {
    Program program;
} AssembleResult;

AssembleResult assemble(Node *root);