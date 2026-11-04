#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "err_enum.h"
#include "logging.c"
#include "sound_queue.h"

// DONT CHANGE FORMAT OF THIS SEGMENT, OR INSTRUCTIONS PARSER COULD BREAK
#define elem_t               double
#define ELEM_T_FORMAT_STRING "%lf"
#include "stack.c"

#define REG_COUNT        9
#define reg_t            uint8_t
#define offset_t         int64_t
#define MEM_SIZE         500000
#define SOUND_QUEUE_SIZE 3000
// ------------------------------------------------------

// #define SOUNDS_ENABLED
#define FPS_LIMIT_ENABLED

#ifndef CPU_STATE_TYPEDEF_H
#define CPU_STATE_TYPEDEF_H
typedef struct spu_state {
  uint8_t *ip;
  double registers[REG_COUNT];
  uint8_t *code;
  size_t code_size;
  stack_t *stack;
  elem_t *memory;
  bool is_running;
  size_t screen_h;
  size_t screen_w;
  size_t fps;
  struct timespec frame_start;
  struct timespec frame_end;
  sound_queue s_queue;
  double spu_start_time;
} spu_state;
#endif

#include "../instructions.h"

// -------------------Prototypes-------------------------------------------

void print_help(const char *argv_0);
void spu_init(spu_state *state, uint8_t *code, size_t size);
void spu_run(spu_state *state);
void spu_destruct(spu_state *state);
status_code create_buffer_with_file_content(
    const char *filename, uint8_t **ptr, size_t *size);
ssize_t get_file_size(int fd);
int read_from_file(uint8_t *buf, size_t size, const int fd);
void log_err_status_code(spu_state *state, status_code err);
void spu_dump(spu_state *state);
void play_sounds(spu_state *state);

// -------------------Util-functions-for-instructions----------------------

void read_arg(spu_state *state, uint8_t *dest, size_t size) {
  for (size_t i = 0; i < size; i += 1) {
    if (state->ip >= state->code + state->code_size) {
      log_err_status_code(state, IP_OUT_OF_CODE);
      exit(IP_OUT_OF_CODE);
    }
    *dest = *(state->ip);
    dest += 1;
    state->ip += 1;
  }
}

void read_value_arg(spu_state *state, elem_t *dest) {
  read_arg(state, (uint8_t *)dest, sizeof(elem_t));
}

void read_reg_arg(spu_state *state, reg_t *dest) {
  read_arg(state, (uint8_t *)dest, sizeof(reg_t));
  if (*dest < 0 || *dest >= REG_COUNT) {
    log_err_status_code(state, INVALID_REG_INDEX);
    exit(INVALID_REG_INDEX);
  }
}

void read_offset_arg(spu_state *state, offset_t *dest) {
  read_arg(state, (uint8_t *)dest, sizeof(offset_t));
}

void jump(spu_state *state, offset_t offset) {
  uint8_t *new_ip = state->ip + offset;
  $info("Trying to jump to code[ 0x%lx ]\n", new_ip - state->code);
  if (new_ip < state->code || new_ip >= state->code + state->code_size) {
    log_err_status_code(state, INVALID_JMP_OFFSET);
    exit(INVALID_JMP_OFFSET);
  }
  state->ip = new_ip;
}

elem_t get_mem(spu_state *state, size_t index) {
  $info("Trying to read memory[ 0x%lx ]\n", index);
  if (index < 0 || index >= MEM_SIZE) {
    log_err_status_code(state, INVALID_MEM_INDEX);
    exit(INVALID_MEM_INDEX);
  }
  return state->memory[index];
}

void put_mem(spu_state *state, size_t index, elem_t elem) {
  $info("Trying to write memory[ 0x%lx ]\n", index);
  if (index < 0 || index >= MEM_SIZE) {
    log_err_status_code(state, INVALID_MEM_INDEX);
    exit(INVALID_MEM_INDEX);
  }
  state->memory[index] = elem;
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
// m - memory (addr from register)
// M - memory (constant addr)
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

void __spu__push__m(spu_state *state) {
  reg_t reg = 0;
  read_reg_arg(state, &reg);
  stack_push(state->stack, get_mem(state, (size_t)state->registers[reg]));
}

void __spu__push__M(spu_state *state) {
  elem_t a = 0;
  read_value_arg(state, &a);
  stack_push(state->stack, get_mem(state, (size_t)a));
}

void __spu__pop__r(spu_state *state) {
  reg_t reg = 0;
  read_reg_arg(state, &reg);
  $info("Popping to reg: %lu\n", reg);
  state->registers[reg] = stack_pop(state->stack);
}

void __spu__pop__m(spu_state *state) {
  reg_t reg = 0;
  read_reg_arg(state, &reg);
  put_mem(state, (size_t)state->registers[reg], stack_pop(state->stack));
}

void __spu__pop__M(spu_state *state) {
  elem_t a = 0;
  read_value_arg(state, &a);
  put_mem(state, (size_t)a, stack_pop(state->stack));
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
  if (a < 0) {
    jump(state, offset - sizeof(offset_t));
  }
}

void __spu__hlt__(spu_state *state) { state->is_running = false; }

void __spu__get__(spu_state *state) {
  elem_t a = 0;
  scanf(ELEM_T_FORMAT_STRING, &a);
  stack_push(state->stack, a);
}

void __spu__call__o(spu_state *state) {
  offset_t offset = 0;
  read_offset_arg(state, &offset);
  stack_push(state->stack, (elem_t)(size_t)state->ip);
  jump(state, offset - sizeof(offset_t));
}

void __spu__ret__(spu_state *state) {
  offset_t offset = (offset_t)stack_pop(state->stack) - (offset_t)state->ip;
  jump(state, offset);
}

void __spu__draw__(spu_state *state) {

  #ifdef FPS_LIMIT_ENABLED
  clock_gettime(CLOCK_MONOTONIC, &state->frame_end);
  int64_t frame_time =
      (state->frame_end.tv_sec * 1000000000 + state->frame_end.tv_nsec) -
      (state->frame_start.tv_sec * 1000000000 + state->frame_start.tv_nsec);
  if (frame_time < 1000000000 / state->fps) {
    struct timespec request = {0, 1000000000 / state->fps - frame_time};
    nanosleep(&request, NULL);
  }

  clock_gettime(CLOCK_MONOTONIC, &state->frame_start);
  #endif

  printf("\033[0;0H");
  for (int i = 0; i < state->screen_h; i += 1) {
    for (int j = 0; j < state->screen_w; j += 1) {
      uint64_t rgb = state->memory[i * state->screen_w + j];
      printf("\x1b[48;2;%lu;%lu;%lum \x1b[0m",
          rgb & 0xff,
          (rgb >> 8) & 0xff,
          (rgb >> 16) & 0xff);
    }
    puts("");
  }
  fflush(stdout);
}

void __spu__setscreensize__(spu_state *state) {
  state->screen_h = (size_t)stack_pop(state->stack);
  state->screen_w = (size_t)stack_pop(state->stack);
}

void __spu__play__(spu_state *state) {
  double start = stack_pop(state->stack);
  double dur   = stack_pop(state->stack);
  double freq  = stack_pop(state->stack);

  if (put_sound(&state->s_queue, (sound){freq, dur, start}) != 0) {
    log_err_status_code(state, SOUND_QUEUE_IS_FULL);
  }
}

void __spu__setfps__(spu_state *state) {
  state->fps = (size_t)stack_pop(state->stack);
}

void __spu__sqrt__(spu_state *state) {
  double a = stack_pop(state->stack);
  stack_push(state->stack, sqrt(a));
}

void __spu__jz__o(spu_state *state) {
  offset_t offset = 0;
  read_offset_arg(state, &offset);
  elem_t a = stack_pop(state->stack);
  if (a * a <= 0.00000000001) {
    jump(state, offset - sizeof(offset_t));
  }
}

void __spu__jnz__o(spu_state *state) {
  offset_t offset = 0;
  read_offset_arg(state, &offset);
  elem_t a = stack_pop(state->stack);
  if (a * a > 0.00000000001) {
    jump(state, offset - sizeof(offset_t));
  }
}

void __spu__jg__o(spu_state *state) {
  offset_t offset = 0;
  read_offset_arg(state, &offset);
  elem_t a = stack_pop(state->stack);
  if (a > 0) {
    jump(state, offset - sizeof(offset_t));
  }
}

void __spu__dump__(spu_state *state) { spu_dump(state); }

void __spu__dup__(spu_state *state) {
  elem_t a = stack_pop(state->stack);
  stack_push(state->stack, a);
  stack_push(state->stack, a);
}

void __spu__jge__o(spu_state *state) {
  offset_t offset = 0;
  read_offset_arg(state, &offset);
  elem_t a = stack_pop(state->stack);
  if (a >= 0) {
    jump(state, offset - sizeof(offset_t));
  }
}

void __spu__jle__o(spu_state *state) {
  offset_t offset = 0;
  read_offset_arg(state, &offset);
  elem_t a = stack_pop(state->stack);
  if (a <= 0) {
    jump(state, offset - sizeof(offset_t));
  }
}

void __spu__sin__(spu_state *state) {
  elem_t a = stack_pop(state->stack);
  stack_push(state->stack, sin(a));
}

void __spu__floor__(spu_state *state) {
  elem_t a = stack_pop(state->stack);
  stack_push(state->stack, floor(a));
}

// ------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IOFBF, 1000000000);
  // setvbuf(stdout, NULL, _IONBF, 0);
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
    log_err_status_code(NULL, ret);
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

void log_err_status_code(spu_state *state, status_code err) {
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
    $error("Invalid opcode\n");
    break;
  case STATUS_OK:
    break;
  case SOUND_QUEUE_IS_FULL:
    $error("Sound queue is full\n");
    break;
  case INVALID_MEM_INDEX:
  case IP_OUT_OF_CODE:
    break;
  }
  spu_dump(state);
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
  state->memory = (elem_t *)calloc(MEM_SIZE, sizeof(elem_t));

  struct timespec cur_time_s = {};
  clock_gettime(CLOCK_MONOTONIC, &cur_time_s);
  state->spu_start_time =
      cur_time_s.tv_sec + (double)cur_time_s.tv_nsec / 1000000000;

  init_sound_queue(&state->s_queue, SOUND_QUEUE_SIZE);

  state->fps = 30;
}

void spu_run(spu_state *state) {
  state->is_running = true;
  while (state->is_running && state->ip < state->code + state->code_size) {

    #ifdef SOUNDS_ENABLED
    play_sounds(state);
    stack_dump(state->stack);
    spu_dump(state);
    #endif

    opcode_t opcode = *(opcode_t *)state->ip;
    $info("Next opcode: 0x%02x on [0x%lx]\n",
        opcode,
        (size_t)(state->ip - state->code));
    state->ip += sizeof(opcode_t);
    switch (opcode) {
#include "switch_case.c"
    default:
      log_err_status_code(state, INVALID_OPCODE);
      exit(INVALID_OPCODE);
    }
  }
}

void spu_destruct(spu_state *state) {
  stack_destruct(state->stack);
  free(state->stack);
  free(state->memory);

  destruct_sound_queue(&state->s_queue);
}

void play_sounds(spu_state *state) {
  struct timespec cur_time_s = {};
  clock_gettime(CLOCK_MONOTONIC, &cur_time_s);
  double cur_time = cur_time_s.tv_sec +
                    (double)cur_time_s.tv_nsec / 1000000000 -
                    state->spu_start_time;

  sound s = {};
  if (top_sound(&state->s_queue, &s) == 0) {
    if (s.start <= cur_time) {
      // $error("Playing sound: freq: %lu, dur: %lu\n",
      //     (uint64_t)s.freq,
      //     (uint64_t)(s.dur * 1000));
      printf("\033[10;%lu]\033[11;%lu]\a",
          (uint64_t)s.freq,
          (uint64_t)(s.dur * 1000));
      fflush(stdout);
      pop_sound(&state->s_queue, &s);
    }
  }
}

void spu_dump(spu_state *state) {
  $log("----------------SPU-dump----------------------------\n");
  uint8_t *begin = state->ip - 10;
  begin          = (begin >= state->code ? begin : state->code);
  uint8_t *end   = state->ip + 10;
  end =
      (end <= state->code + state->code_size ? end
                                             : state->code + state->code_size);

  $log(GREEN "Code:" RESET "\n... ");
  for (uint8_t *iter = begin; iter != end; iter += 1) {
    if (iter == state->ip) {
      $log(MAGENTA "%02x ", *iter);
    } else {
      $log(WHITE "%02x " RESET, *iter);
    }
  }
  $log(" ...\n    ");
  for (uint8_t *iter = begin; iter != end; iter += 1) {
    if (iter == state->ip) {
      $log(MAGENTA " ↑ ");
    } else {
      $log("   ");
    }
  }
  $log("\n");

  size_t ip = state->ip - state->code;
  $log(MAGENTA "IP" RESET " \t = " YELLOW "0x%lx\n" RESET,
      (size_t)(state->ip - state->code));
  for (size_t i = 0; i < REG_COUNT; i += 1) {
    $log(GREEN "r%lu" RESET " \t = " YELLOW "%lf\n" RESET,
        i,
        state->registers[i]);
  }
  stack_dump_values(state->stack);
}
