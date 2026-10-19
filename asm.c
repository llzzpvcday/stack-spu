#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "instructions.h"

typedef enum err_t {
  OK               = 0,
  INVALID_OPTION   = -1,
  OPENING_FILE_ERR = -2,
} err_t;

typedef enum inst_arg_parser_err_t {
  NO_MORE_ARGS      = 1,
  INST_ARG_OK       = 0,
  INVALID_REG_INDEX = -1,
  TOO_MUCH_ARGS     = -2,
  INST_NOT_FOUND    = -3,
} inst_arg_parser_err_t;

typedef struct parser_state {
  FILE *src_file;
  FILE *bc_file;
  size_t byte_cnt;
} parser_state;

typedef enum arg_type_t {
  REG_TYPE    = 'r',
  VALUE_TYPE  = 'v',
  OFFSET_TYPE = 'o',
  LABEL_TYPE  = 'l'
} arg_type_t;

typedef struct inst_arg_t {
  arg_type_t arg_type;
  reg_t r_arg;
  offset_t o_arg;
  elem_t v_arg;
  char *label;
} inst_arg_t;

void print_help(const char *argv_0);
err_t argv_parse(
    int argc, char *argv[], char **src_filename, char **bytecode_filename);
err_t open_files(
    parser_state *state, char *src_filename, char *bytecode_filename);
void parser_destruct(parser_state *state);
void start_parsing(parser_state *state);
long long inst_match(parser_state *state,
    const char *name,
    inst_arg_t *arg_list,
    long arg_list_size);
void bc_write(parser_state *state, uint8_t *ptr, size_t size, size_t count);
void inst_write(parser_state *state,
    opcode_t opcode,
    inst_arg_t *arg_list,
    size_t arg_list_size);
void log_inst_arg_parser_err(inst_arg_parser_err_t err, long line);
// void add_label(parser_state *state, const char *name);

int main(int argc, char *argv[]) {
  char *src_filename      = NULL;
  char *bytecode_filename = NULL;

  if (argv_parse(argc, argv, &src_filename, &bytecode_filename) != OK) {
    print_help(argv[0]);
    return INVALID_OPTION;
  }

  parser_state state = {};

  if (open_files(&state, src_filename, bytecode_filename) != OK) {
    fprintf(stderr, "Opening file error: %s\n", strerror(errno));
    return OPENING_FILE_ERR;
  }

  start_parsing(&state);
  parser_destruct(&state);
}

err_t argv_parse(
    int argc, char *argv[], char **src_filename, char **bytecode_filename) {
  int opt = 0;
  while ((opt = getopt(argc, argv, "i:o:")) != -1) {
    switch (opt) {
    case 'i':
      *src_filename = optarg;
      break;
    case 'o':
      *bytecode_filename = optarg;
      break;
    default:
      return INVALID_OPTION;
    }
  }
  return OK;
}

void print_help(const char *argv_0) {
  fprintf(stderr,
      "Usage: %s [-i <input filename>] [-o <output filename>]\n"
      "Utility for converting file in assembly language to bytecode for spu\n"
      "\n"
      "Options:\n"
      // "-h"
      // "    display this help and exit"
      "-i\n"
      "    Assembly file. Default: `stdin`\n"
      "-o\n"
      "    Output bytecode file. Default: `stdout`\n",
      argv_0);
}

err_t open_files(
    parser_state *state, char *src_filename, char *bytecode_filename) {

  if (src_filename == NULL) {
    state->src_file = stdin;
  } else {
    state->src_file = fopen(src_filename, "r");
    if (state->src_file == NULL) {
      return OPENING_FILE_ERR;
    }
  }

  if (bytecode_filename == NULL) {
    state->bc_file = stdout;
  } else {
    state->bc_file = fopen(bytecode_filename, "wb");
    if (state->bc_file == NULL) {
      return OPENING_FILE_ERR;
    }
  }

  return OK;
}

void parser_destruct(parser_state *state) {
  fclose(state->src_file);
  fclose(state->bc_file);
}

void start_parsing(parser_state *state) {
  long line_counter = 0;
  while (feof(state->src_file) == 0 && ferror(state->src_file) == 0 &&
         ferror(state->bc_file) == 0) {
    line_counter += 1;
    char buf[64] = "";

    // Labels
    if (fscanf(state->src_file, ":%63[a-z]", buf) > 0) {
      // add_label(state, buf);
      fscanf(state->src_file, "%*[^\n]\n");
      continue;
    }

    // Instruction
    if (fscanf(state->src_file, "%63[a-z]", buf) <= 0) {
      fscanf(state->src_file, "%*[^\n]\n");
      continue;
    }

    // Args
    size_t arg_counter      = 0;
    inst_arg_t arg_list[64] = {};
    while (true) {
      if (arg_counter >= sizeof(arg_list) / sizeof(inst_arg_t)) {
        log_inst_arg_parser_err(TOO_MUCH_ARGS, line_counter);
        return;
      }
      inst_arg_parser_err_t ret = inst_arg_parse(state, &arg_list[arg_counter]);
      if (ret == NO_MORE_ARGS)
        break;
      if (ret != INST_ARG_OK) {
        log_inst_arg_parser_err(ret, line_counter);
        return;
      }
      arg_counter += 1;
    }

    // printf("Line: %d, inst_name: %s, arg_counter: %d\n",
    //     line_counter,
    //     buf,
    //     arg_counter);
    // for (int t = 0; t < arg_counter; t += 1) {
    //   printf("%3d %c\n", t, arg_list[t].arg_type);
    // }
    // puts("");

    long long res = inst_match(state, buf, arg_list, arg_counter);
    if (res < 0) {
      log_inst_arg_parser_err(INST_NOT_FOUND, line_counter);
      return;
    }

    inst_write(state, opcode_mapping[res].opcode, arg_list, arg_counter);
  }
}

inst_arg_parser_err_t inst_arg_parse(
    parser_state *state, inst_arg_t *inst_arg) {
  unsigned long long reg = 0;
  long long offset       = 0;
  char label_buf[64]     = "";
  elem_t value           = 0;

  fscanf(state->src_file, "%*[ ]"); // read until next not space char

  if (fscanf(state->src_file, "r%llu", &reg) > 0) {
    if (reg >= REG_COUNT) {
      return INVALID_REG_INDEX;
    }
    inst_arg->arg_type = REG_TYPE;
    inst_arg->r_arg    = reg;
  } else if (fscanf(state->src_file, ">%lld", &offset) > 0) {
    inst_arg->arg_type = OFFSET_TYPE;
    inst_arg->o_arg    = offset;
  } else if (fscanf(state->src_file, ":%63[a-z]", label_buf) > 0) {
    inst_arg->arg_type = LABEL_TYPE;
    // TODO: label arg
  } else if (fscanf(state->src_file, ELEM_T_FORMAT_STRING, &value) > 0) {
    inst_arg->arg_type = VALUE_TYPE;
    inst_arg->v_arg    = value;
  } else {
    return NO_MORE_ARGS;
  }

  return INST_ARG_OK;
}

long long inst_match(parser_state *state,
    const char *name,
    inst_arg_t *arg_list,
    long arg_list_size) { // returns -1 if instruction not found, else -
                          // instruction index

  for (int i = 0; i < INST_COUNT; i += 1) {
    if (strcmp(name, opcode_mapping[i].name) == 0) {

      const char *arg_types = opcode_mapping[i].args;
      bool match            = true;

      for (int j = 0; j < arg_list_size; j += 1) {
        if (arg_types[j] == '\0' || arg_types[j] != arg_list[j].arg_type) {
          match = false;
          break;
        }
      }

      if (match) {
        if (arg_types[arg_list_size] == '\0') {
          return i;
        }
      }
    }
  }

  return -1;
}

void bc_write(parser_state *state, uint8_t *ptr, size_t size, size_t count) {
  state->byte_cnt += size * count;
  fwrite(ptr, size, count, state->bc_file);
}

void inst_write(parser_state *state,
    opcode_t opcode,
    inst_arg_t *arg_list,
    size_t arg_list_size) {
  bc_write(state, &opcode, sizeof(opcode_t), 1);

  for (int i = 0; i < arg_list_size; i += 1) {
    switch (arg_list[i].arg_type) {
    case REG_TYPE:
      bc_write(state, (uint8_t *)&arg_list[i].r_arg, sizeof(reg_t), 1);
      break;
    case VALUE_TYPE:
      bc_write(state, (uint8_t *)&arg_list[i].v_arg, sizeof(elem_t), 1);
      break;
    case OFFSET_TYPE:
      bc_write(state, (uint8_t *)&arg_list[i].o_arg, sizeof(offset_t), 1);
      break;
    case LABEL_TYPE:
      // TODO:
      break;
    }
  }
}

void log_inst_arg_parser_err(inst_arg_parser_err_t err, long line) {
  switch (err) {
  case INVALID_REG_INDEX:
    fprintf(stderr, "Invalid register index on line: %ld\n", line);
    break;
  case TOO_MUCH_ARGS:
    fprintf(stderr, "Too much arguments for instruction on line: %ld\n", line);
    break;
  case INST_NOT_FOUND:
    fprintf(stderr, "Unexpected instruction on line: %ld\n", line);
    break;
  case NO_MORE_ARGS:
  case INST_ARG_OK:
    break;
  }
}
