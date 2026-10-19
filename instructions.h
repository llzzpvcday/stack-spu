// This file was automatically gnerated from spu.c with header_generate.py
#define REG_COUNT            (uint8_t)9
#define reg_t                typeof(REG_COUNT)
#define offset_t             int64_t
#define elem_t               double
#define ELEM_T_FORMAT_STRING "%lf"
#define INST_COUNT           10
#define opcode_t             uint8_t
typedef struct opcode_mapping_t {
  const char *name;
  opcode_t opcode;
  const char *args;
} opcode_mapping_t;
opcode_mapping_t opcode_mapping[]{
    {"push", 0, "r"},
    {"push", 1, "v"},
    {"pop",  2, "r"},
    {"add",  3, "" },
    {"sub",  4, "" },
    {"mul",  5, "" },
    {"div",  6, "" },
    {"out",  7, "" },
    {"jmp",  8, "o"},
    {"hlt",  9, "" },
};
