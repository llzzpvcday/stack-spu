#include "err_enum.h"
#include "logging.c"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// DONT CHANGE FORMAT OF THIS SEGMENT, OR INSTRUCTIONS PARSER COULD BREAK
#define elem_t               double
#define ELEM_T_FORMAT_STRING "%lf"
#include "stack.c"

#define REG_COUNT (uint8_t)9
#define reg_t     typeof(REG_COUNT)
#define offset_t  int64_t
// ------------------------------------------------------

#ifndef CPU_STATE_TYPEDEF_H
#define CPU_STATE_TYPEDEF_H
typedef struct spu_state {
  uint8_t *ip;
  double registers[REG_COUNT];
  uint8_t *code;
  size_t code_size;
  stack_t *stack;
  bool is_running;
} spu_state;
#endif /* end of include guard: CPU_STATE_TYPEDEF_H */

#include "instructions.h"

// -------------------Prototypes-------------------------------------------

void print_help(const char *argv_0);
void spu_init(spu_state *state, uint8_t *code, size_t size);
void spu_run(spu_state *state);
void spu_destruct(spu_state *state);
status_code create_buffer_with_file_content(
    const char *filename, uint8_t **ptr, size_t *size);
ssize_t get_file_size(int fd);
int read_from_file(uint8_t *buf, size_t size, const int fd);
void log_err_status_code(status_code err);
// TODO: Spu dump

// -------------------Util-functions-for-instructions----------------------

void read_arg(spu_state *state, uint8_t *dest, size_t size) {
  for (size_t i = 0; i < size; i += 1) {
    *dest = *(state->ip);
    dest += 1;
    state->ip += 1;
  }
}

void read_value_arg(spu_state *state, elem_t *dest) {
  read_arg(state, (uint8_t *)dest, sizeof(elem_t));
}

void read_reg_arg(spu_state *state, reg_t *dest) {
  reg_t reg = 0;
  read_arg(state, (uint8_t *)&reg, sizeof(reg_t));
  if (reg < 0 || reg >= REG_COUNT) {
    log_err_status_code(INVALID_REG_INDEX);
    exit(INVALID_REG_INDEX);
  }
}

void read_offset_arg(spu_state *state, offset_t *dest) {
  read_arg(state, (uint8_t *)dest, sizeof(offset_t));
}

void jump(spu_state *state, offset_t offset) {
  uint8_t *new_ip = state->ip + offset;
  $info("Trying to jump to code[ %ld ]\n", new_ip - state->code);
  if (new_ip < state->code || new_ip >= state->code + state->code_size) {
    log_err_status_code(INVALID_JMP_OFFSET);
    exit(INVALID_JMP_OFFSET);
  }
  state->ip = new_ip;
}

// -------------------Instructions-----------------------------------------
// Instruction function name format:
// __spu__<instruction name>__<list of arguments characters>
//
// instruction name contains only lowercase characters from a to z. Names `inf`
// and `nan` is forbidden, because could be interpreted as float
//
// arguments characters:
// r - register
// v - value (of elem_t type)
// o - offset (of offset_t type)
// Example:
// __spu__leet__rv
// this function have two arguments: register(r) and elem_t value(v)

void __spu__push__r(spu_state *state) {
  reg_t reg = 0;
  read_reg_arg(state, &reg);
  stack_push(state->stack, state->registers[reg]);
}

void __spu__push__v(spu_state *state) {
  elem_t t = 0;
  read_value_arg(state, &t);
  stack_push(state->stack, t);
}

void __spu__pop__r(spu_state *state) {
  reg_t reg = 0;
  read_reg_arg(state, &reg);
  state->registers[reg] = stack_pop(state->stack);
}

void __spu__add__(spu_state *state) {
  elem_t a = stack_pop(state->stack);
  elem_t b = stack_pop(state->stack);
  stack_push(state->stack, a + b);
}

void __spu__sub__(spu_state *state) {
  elem_t a = stack_pop(state->stack);
  elem_t b = stack_pop(state->stack);
  stack_push(state->stack, a - b);
}

void __spu__mul__(spu_state *state) {
  elem_t a = stack_pop(state->stack);
  elem_t b = stack_pop(state->stack);
  stack_push(state->stack, a * b);
}

void __spu__div__(spu_state *state) {
  elem_t a = stack_pop(state->stack);
  elem_t b = stack_pop(state->stack);
  stack_push(state->stack, a / b);
}

void __spu__out__(spu_state *state) {
  elem_t a = stack_pop(state->stack);
  printf(ELEM_T_FORMAT_STRING "\n", a);
}

void __spu__jmp__o(spu_state *state) {
  offset_t offset = 0;
  read_offset_arg(state, &offset);
  jump(state, offset - sizeof(offset_t));
}

void __spu__jl__o(spu_state *state) {
  offset_t offset = 0;
  read_offset_arg(state, &offset);
  elem_t a = stack_pop(state->stack);
  if (a <= 0) {
    jump(state, offset - sizeof(offset_t));
  }
}

void __spu__hlt__(spu_state *state) { state->is_running = false; }

void __spu__inp__(spu_state *state) {
  elem_t a = 0;
  scanf(ELEM_T_FORMAT_STRING, &a);
  stack_push(state->stack, a);
}

// ------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  log_init();

  if (argc < 2) {
    print_help(argv[0]);
    return -1;
  }

  char *bytecode_filename = argv[1];

  uint8_t *code = NULL;
  size_t size   = 0;
  status_code ret =
      create_buffer_with_file_content(bytecode_filename, &code, &size);

  if (ret < 0) {
    log_err_status_code(ret);
    return ret;
  }

  spu_state state = {};
  spu_init(&state, code, size);

  spu_run(&state);

  free(code);
  spu_destruct(&state);
  log_close();
}

void print_help(const char *argv_0) {
  fprintf(stderr, "Usage: %s <bytecode file>\n", argv_0);
}

void log_err_status_code(status_code err) {
  char *errno_str = strerror(errno);
  switch (err) {
  case FILE_OPEN_ERR:
    $error("Opening file error: %s\n", errno_str);
    break;
  case FILE_STAT_ERR:
    $error("Getting file stat error: %s\n", errno_str);
    break;
  case ALLOCATION_ERR:
    $error("Buffer allocation error\n");
    break;
  case FILE_READ_ERR:
    $error("Reading error\n");
    break;
  case INVALID_REG_INDEX:
    $error("Invalid register index in bytecode\n");
    break;
  case INVALID_JMP_OFFSET:
    $error("Invalid jump offset in bytecode\n");
    break;
  case INVALID_OPCODE:
    $error("Invalid opcode in bytecode\n");
    break;
  case STATUS_OK:
    break;
  }
}

ssize_t get_file_size(int fd) {
  struct stat file_stat = {};
  int ret               = fstat(fd, &file_stat);

  if (ret < 0) {
    return FILE_STAT_ERR;
  }

  return file_stat.st_size;
}

int read_from_file(uint8_t *buf, size_t size, const int fd) {
  size_t read_counter = 0;

  while (read_counter < size) {
    ssize_t ret = read(fd, buf + read_counter, size - read_counter);
    if (ret < 0)
      return -1;

    read_counter += ret;
  }

  return 0;
}

status_code create_buffer_with_file_content(
    const char *filename, uint8_t **ptr, size_t *size) {

  int fd = open(filename, O_RDONLY);

  if (fd < 0)
    return FILE_OPEN_ERR;

  *size = get_file_size(fd);
  if (*size < 0)
    return FILE_STAT_ERR;

  *ptr = (uint8_t *)calloc(*size, 1);
  if (*ptr == NULL)
    return ALLOCATION_ERR;

  if (read_from_file(*ptr, *size, fd) < 0)
    return FILE_READ_ERR;

  close(fd);

  return STATUS_OK;
}

void spu_init(spu_state *state, uint8_t *code, size_t size) {
  memset(&state->registers, 0, REG_COUNT);
  state->code      = code;
  state->code_size = size;
  state->ip        = code;
  state->stack     = (stack_t *)calloc(sizeof(stack_t), 1);
  stack_init(state->stack, 16);
}

void spu_run(spu_state *state) {
  state->is_running = true;
  while (state->is_running && state->ip < state->code + state->code_size) {
    // stack_dump(state->stack);
    opcode_t opcode = *(opcode_t *)state->ip;
    $info("Next opcode: %d\n", opcode);
    state->ip += sizeof(opcode_t);
    switch (opcode) {
#include "switch_case.c"
    default:
      log_err_status_code(INVALID_OPCODE);
      exit(INVALID_OPCODE);
    }
  }
}

void spu_destruct(spu_state *state) {
  stack_destruct(state->stack);
  free(state->stack);
}
