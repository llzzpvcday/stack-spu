#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../instructions.h"

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
  LABEL_DUPLICATE   = -4,
} inst_arg_parser_err_t;

typedef struct fixup {
  size_t label_index;
  size_t fixup_offset;
} fixup;

typedef struct label {
  char *name;
  offset_t offset;
} label;

typedef struct labels_table {
  label *labels_list;
  size_t labels_list_size;
  size_t labels_list_cap;
  fixup *fixup_list;
  size_t fixup_list_size;
  size_t fixup_list_cap;
} lables_table;

#define BASIC_LIST_CAPACITY 16

typedef struct parser_state {
  FILE *src_file;
  FILE *bc_file;
  size_t byte_cnt;
  lables_table labels;
  bool fixup_required;
} parser_state;

typedef enum arg_type_t {
  REG_TYPE    = 'r',
  VALUE_TYPE  = 'v',
  OFFSET_TYPE = 'o',
} arg_type_t;

typedef struct inst_arg_t {
  arg_type_t arg_type;
  reg_t r_arg;
  offset_t o_arg;
  elem_t v_arg;
  size_t l_arg;
  bool is_label;
} inst_arg_t;

void print_help(const char *argv_0);
err_t argv_parse(
    int argc, char *argv[], char **src_filename, char **bytecode_filename);
err_t open_files(
    parser_state *state, char *src_filename, char *bytecode_filename);
void spu_init(parser_state *state);
void parser_destruct(parser_state *state);
void start_parsing(parser_state *state);
inst_arg_parser_err_t inst_arg_parse(parser_state *state, inst_arg_t *inst_arg);
int start_fixup(parser_state *state);
long long inst_match(parser_state *state,
    const char *name,
    inst_arg_t *arg_list,
    size_t arg_list_size);
void bc_write(parser_state *state, uint8_t *ptr, size_t size, size_t count);
int add_fixup(parser_state *state, size_t label_index);
int add_label(parser_state *state, const char *label_name, size_t offset);
int get_label_index(parser_state *state, const char *label_name);
void inst_write(parser_state *state,
    opcode_t opcode,
    inst_arg_t *arg_list,
    size_t arg_list_size);
void log_inst_arg_parser_err(inst_arg_parser_err_t err, long line);

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

  spu_init(&state);

  start_parsing(&state);

  if (state.fixup_required) {
    int ret = start_fixup(&state);
    if (ret != 0) {
      fprintf(
          stderr, "Undefined label: %s\n", state.labels.labels_list[ret].name);
    }
  }

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

void spu_init(parser_state *state) {
  state->labels.labels_list =
      (label *)calloc(BASIC_LIST_CAPACITY, sizeof(label));
  state->labels.labels_list_cap = BASIC_LIST_CAPACITY;
  state->labels.fixup_list =
      (fixup *)calloc(BASIC_LIST_CAPACITY, sizeof(fixup));
  state->labels.fixup_list_cap = BASIC_LIST_CAPACITY;
  state->fixup_required        = false;
}

void parser_destruct(parser_state *state) {
  for (size_t i = 0; i < state->labels.labels_list_size; i += 1) {
    free(state->labels.labels_list[i].name);
  }

  free(state->labels.labels_list);
  free(state->labels.fixup_list);
  fclose(state->src_file);
  fclose(state->bc_file);
}

void start_parsing(parser_state *state) {
  while (feof(state->src_file) == 0 && ferror(state->src_file) == 0 &&
         ferror(state->bc_file) == 0) {
    char buf[64] = "";

    // Labels
    if (fscanf(state->src_file, ";%63[a-z]", buf) > 0) {
      printf("Adding label: %s\n", buf);
      int label_index = add_label(state, buf, state->byte_cnt);
      printf("New index: %d\n", label_index);
      if (label_index == -1) {
        log_inst_arg_parser_err(LABEL_DUPLICATE, ftell(state->src_file));
        return;
      }
      continue;
    }

    // Instruction
    if (fscanf(state->src_file, " %63[a-z]", buf) <= 0) {
      fprintf(stderr,
          "Unexpected symbol at position: %ld\n",
          ftell(state->src_file));
      return;
    }

    // Args
    size_t arg_counter      = 0;
    inst_arg_t arg_list[64] = {};
    while (true) {
      if (arg_counter >= sizeof(arg_list) / sizeof(inst_arg_t)) {
        log_inst_arg_parser_err(TOO_MUCH_ARGS, ftell(state->src_file));
        return;
      }
      inst_arg_parser_err_t ret = inst_arg_parse(state, &arg_list[arg_counter]);
      if (ret == NO_MORE_ARGS)
        break;
      if (ret != INST_ARG_OK) {
        log_inst_arg_parser_err(ret, ftell(state->src_file));
        return;
      }
      arg_counter += 1;
    }

    long long res = inst_match(state, buf, arg_list, arg_counter);
    if (res < 0) {
      log_inst_arg_parser_err(INST_NOT_FOUND, ftell(state->src_file));
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
    inst_arg->r_arg    = (reg_t)reg;
  } else if (fscanf(state->src_file, ">%lld", &offset) > 0) {
    inst_arg->arg_type = OFFSET_TYPE;
    inst_arg->o_arg    = offset;
  } else if (fscanf(state->src_file, ":%63[a-z]", label_buf) > 0) {
    inst_arg->arg_type = OFFSET_TYPE;
    inst_arg->l_arg    = get_label_index(state, label_buf);
    inst_arg->is_label = true;
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
    size_t arg_list_size) { // returns -1 if instruction not found, else -
                            // instruction index
  printf("Name: %s\nArgs: ", name);
  for (int i = 0; i < arg_list_size; i += 1) {
    printf("%c,", arg_list[i].arg_type);
  }
  puts("");

  for (int i = 0; i < INST_COUNT; i += 1) {
    // TODO: Inst names hashing and binsearch
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

      if (arg_list[i].is_label) {

        size_t label_index = arg_list[i].l_arg;
        if (state->labels.labels_list[label_index].offset != -1) {
          offset_t offset =
              state->labels.labels_list[label_index].offset - state->byte_cnt;
          bc_write(state, (uint8_t *)&offset, sizeof(offset_t), 1);
        } else {
          state->fixup_required = true;
          add_fixup(state, arg_list[i].l_arg);
          uint8_t buf[sizeof(offset_t)] = {};
          bc_write(state, buf, sizeof(offset_t), 1);
          break;
        }
      } else {

        bc_write(state, (uint8_t *)&arg_list[i].o_arg, sizeof(offset_t), 1);
      }
      break;
    }
  }
}

void log_inst_arg_parser_err(inst_arg_parser_err_t err, long position) {
  switch (err) {
  case INVALID_REG_INDEX:
    fprintf(stderr, "Invalid register index at position: %ld\n", position);
    break;
  case TOO_MUCH_ARGS:
    fprintf(stderr,
        "Too much arguments for instruction at position: %ld\n",
        position);
    break;
  case INST_NOT_FOUND:
    fprintf(stderr, "Unexpected instruction at position: %ld\n", position);
    break;
  case LABEL_DUPLICATE:
    fprintf(stderr, "Label duplicate at position: %ld\n", position);
    break;
  case NO_MORE_ARGS:
  case INST_ARG_OK:
    break;
  }
}

int _create_label(parser_state *state, const char *label_name) {
  printf("Creating new label: %s\n", label_name);
  if (state->labels.labels_list_cap <= state->labels.labels_list_size) {

    state->labels.labels_list = (label *)realloc(
        state->labels.labels_list, state->labels.labels_list_cap * 2);

    state->labels.labels_list_cap *= 2;
  }

  state->labels.labels_list[state->labels.labels_list_size] =
      (label){strdup(label_name), -1};
  state->labels.labels_list_size += 1;

  return state->labels.labels_list_size - 1;
}

int _find_label(parser_state *state, const char *label_name) {
  for (int i = 0; i < state->labels.labels_list_size; i += 1) {
    if (strcmp(state->labels.labels_list[i].name, label_name) == 0) {
      return i;
    }
  }
  return -1;
}

int get_label_index(parser_state *state, const char *label_name) {
  printf("Getting label index: %s\n", label_name);
  int ret = _find_label(state, label_name);
  if (ret != -1)
    return ret;

  return _create_label(state, label_name);
}

int add_label(parser_state *state, const char *label_name, size_t offset) {
  int ret = _find_label(state, label_name);
  if (ret != -1) {
    if (state->labels.labels_list[ret].offset != -1) {
      return -1;
    }
  } else
    ret = _create_label(state, label_name);

  state->labels.labels_list[ret].offset = offset;
  return ret;
}

int add_fixup(parser_state *state, size_t label_index) {

  if (label_index >= state->labels.labels_list_size) {
    return -1;
  }

  if (state->labels.fixup_list_size >= state->labels.fixup_list_cap) {
    state->labels.fixup_list = (fixup *)realloc(
        state->labels.fixup_list, state->labels.fixup_list_cap * 2);
    state->labels.fixup_list_cap *= 2;
  }

  state->labels.fixup_list[state->labels.fixup_list_size] =
      (fixup){label_index, state->byte_cnt};
  state->labels.fixup_list_size += 1;

  return state->labels.fixup_list_size - 1;
}

int start_fixup(parser_state *state) {
  for (int i = 0; i < state->labels.fixup_list_size; i += 1) {
    offset_t offset = state->labels.fixup_list[i].fixup_offset;
    size_t index    = state->labels.fixup_list[i].label_index;
    if (state->labels.labels_list[index].offset == -1) {
      return index + 1;
    }
    fseek(state->bc_file, offset, SEEK_SET);

    offset_t dif = state->labels.labels_list[index].offset - offset;
    fwrite((uint8_t *)&dif, sizeof(offset_t), 1, state->bc_file);
  }
  return 0;
}
