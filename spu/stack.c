#include "colors.h"
#include "logging.c"
#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//-------------------------------------Options-------------------------------------------

#define STACK_ALLOCATED_ENOUGH_CHECK // This check only available with glibc
#define STACK_STRUCT_HASH_CHECK
#define STACK_BUF_HASH_CHECK
#define STACK_CANARY_CHECK
// #define STACK_DEBUG

//-------------------------------------Defines-------------------------------------------

#ifdef STACK_CANARY_CHECK
#define ON_CANARY(...) __VA_ARGS__
#else
#define ON_CANARY(...)
#endif

#ifdef STACK_BUF_HASH_CHECK
#define ON_BUF_HASH(...) __VA_ARGS__
#else
#define ON_BUF_HASH(...)
#endif

#ifdef STACK_STRUCT_HASH_CHECK
#define ON_STRUCT_HASH(...) __VA_ARGS__
#else
#define ON_STRUCT_HASH(...)
#endif

#ifdef STACK_DEBUG
#define ON_DEBUG(...) __VA_ARGS__
#else
#define ON_DEBUG(...)
#endif

//-------------------------------------Stack_structure-----------------------------------

#ifndef elem_t
#define elem_t double
#endif

#ifndef ELEM_T_FORMAT_STRING
#define ELEM_T_FORMAT_STRING "%lf"
#endif

ON_CANARY(
    elem_t canary = 0; // Will be random value after first stack_init() call
    bool is_canary_set = false;
)

const size_t STACK_MIN_CAPACITY = 16;
const size_t SHRINK_RATIO       = 4;

typedef struct stack_t {
  ON_CANARY(elem_t canary_begin;)

  elem_t *buf;
  // buf structure:
  // <canary(elem_t)> |  <stack elements (elem_t)>  |  <canary(elem_t)>
  //  sizeof(elem_t)     sizeof(elem_t) * capacity      sizeof(elem_t)
  // if canary disables: just sizeof(elem_t) * capacity
  size_t size;
  size_t capacity;

  ON_STRUCT_HASH(uint64_t struct_hash;)
  ON_BUF_HASH(uint64_t buf_hash;)
  ON_CANARY(elem_t canary_end;)

  ON_DEBUG(
      const char *filename;
      const char *name;
      const char *init_func_name;
      int line;)
} stack_t;

// clang-format off
typedef enum stack_status_code {
                 STACK_OK                     = 0,
                 BUF_NULL_PTR                 = -1,
                 STACK_NULL_PTR               = -2,
                 SIZE_GREATER_THAN_CAP        = -3,
  #ifdef STACK_ALLOCATED_ENOUGH_CHECK
                 ALLOCATED_NOT_ENOUGH         = -4,
  #endif
  ON_CANARY(     STRUCT_CANARY_BEGIN_MISMATCH = -7,)
  ON_CANARY(     STRUCT_CANARY_END_MISMATCH   = -8,)
  ON_CANARY(     BUF_CANARY_BEGIN_MISMATCH    = -9,)
  ON_CANARY(     BUF_CANARY_END_MISMATCH      = -10,)
  ON_STRUCT_HASH(STRUCT_HASH_MISMATCH         = -11,)
  ON_BUF_HASH(   BUF_HASH_MISMATCH            = -12,)
} stack_status_code;
// clang-format on

//--------------------------------Prototypes---------------------------------------------

#if defined(STACK_BUF_HASH_CHECK) | defined(STACK_STRUCT_HASH_CHECK)
uint64_t hash_update(uint64_t hash, uint8_t elem);
void update_hashes(stack_t *stack);
#endif

ON_STRUCT_HASH(
uint64_t stack_struct_hash(stack_t stack);
)

ON_BUF_HASH(
uint64_t stack_buf_hash(stack_t *stack);
)

stack_status_code verify(stack_t *stack);
size_t calc_buf_size(size_t capacity);
void stack_dump(stack_t *stack);
elem_t *calloc_stack_buf(size_t capacity);
elem_t *realloc_stack_buf(elem_t *buf, size_t capacity);
ON_CANARY(void place_buf_canaries(elem_t *buf, size_t capacity);)
void stack_init(stack_t *stack,
    size_t capacity ON_DEBUG(, const char *filename, const char *name, const char *init_func_name, const int line) );
void stack_destruct(stack_t *stack);
void stack_resize(stack_t *stack, size_t new_capacity);
void stack_push(stack_t *stack, elem_t elem);
elem_t stack_pop(stack_t *stack);

//--------------------------------Main---------------------------------------------------

// int main() {
//   log_init();
//   stack_t stack = {};
//   stack_init(&stack, 5 ON_DEBUG(,
//                                 __FILE__,
//                                 "stack1",
//                                 __func__,
//                                 __LINE__));
//
//   stack_push(&stack, 10);
//   stack_push(&stack, 20);
//   stack_push(&stack, 30);
//   stack_push(&stack, 40);
//   stack.buf[stack.capacity] = 123;
//   stack_push(&stack, 100);
//   //
//   // for (int i = 0; i < 30; i += 1) {
//   //   stack_push(&stack, (double)(rand() % 256));
//   //   // stack_dump(&stack, stdout);
//   // }
//   //
//   // for (int i = 0; i < 30; i += 1) {
//   //   if (i == 15) {
//   //     stack.buf[11] = 1337.0;
//   //     // stack.canary_begin = 123;
//   //   }
//   //   printf("Popped: " ELEM_T_FORMAT_STRING "\n", stack_pop(&stack));
//   //   // stack_dump(&stack, stdout);
//   // }
//   //
//   stack_destruct(&stack);
//   log_close();
// }

//---------------------------------------------------------------------------------------

#if defined(STACK_BUF_HASH_CHECK) || defined(STACK_STRUCT_HASH_CHECK)

uint64_t hash_mem(uint8_t *data, size_t size) {
  uint64_t hash = 5381;
  uint8_t *iter = data;
  for (; iter != data + size; iter += 1) {
    hash = ((hash << 5) + hash) + *iter;
  }
  return hash;
}

void update_hashes(stack_t *stack) {

  ON_STRUCT_HASH(
  stack->struct_hash = stack_struct_hash(*stack);
  )

  ON_BUF_HASH(
  stack->buf_hash    = stack_buf_hash(stack);
  )
}
#endif

ON_STRUCT_HASH(
    uint64_t stack_struct_hash(stack_t stack) {
      stack.struct_hash = 0;
      stack.buf_hash    = 0;
      return hash_mem((uint8_t *)&stack, sizeof(stack_t));
    }
)

ON_BUF_HASH(
    uint64_t stack_buf_hash(stack_t *stack) {
      return hash_mem((uint8_t *)stack->buf, stack->size * sizeof(elem_t));
    }
)

stack_status_code verify(stack_t *stack) {
  if (stack == NULL) {
    return STACK_NULL_PTR;
  }

  ON_CANARY(
      if (stack->canary_begin != canary) {
        return STRUCT_CANARY_BEGIN_MISMATCH;
      }

      if (stack->canary_end != canary) {
        return STRUCT_CANARY_END_MISMATCH;
      }
  )

  ON_STRUCT_HASH(
      if (stack_struct_hash(*stack) != stack->struct_hash) {
        return STRUCT_HASH_MISMATCH;
      }
  )

  if (stack->buf == NULL) {
    return BUF_NULL_PTR;
  }

  if (stack->size > stack->capacity) {
    return SIZE_GREATER_THAN_CAP;
  }

#ifdef STACK_ALLOCATED_ENOUGH_CHECK
  if (malloc_usable_size(stack->buf ON_CANARY( - 1)) < calc_buf_size(stack->capacity)) {
    return ALLOCATED_NOT_ENOUGH;
  }
#endif

  ON_CANARY(
      if (*(stack->buf - 1) != canary) {
        return BUF_CANARY_BEGIN_MISMATCH;
      }

      if (*(stack->buf + stack->capacity) != canary) {
        return BUF_CANARY_END_MISMATCH;
      }
  )

  ON_BUF_HASH(
      if (stack_buf_hash(stack) != stack->buf_hash) {
        return BUF_HASH_MISMATCH;
      }
  )

  return STACK_OK;
}

inline size_t calc_buf_size(size_t capacity) {
  return sizeof(elem_t) * capacity ON_CANARY(+ 2 * sizeof(elem_t));
}

inline elem_t *calloc_stack_buf(size_t capacity) {
  return (elem_t *)calloc(calc_buf_size(capacity), 1) ON_CANARY(+ 1);
}

inline elem_t *realloc_stack_buf(elem_t *buf, size_t capacity) {
  elem_t *new_buf = (elem_t *)realloc(buf ON_CANARY(- 1), calc_buf_size(capacity));
  assert(new_buf != NULL);
  return new_buf ON_CANARY(+ 1);
}

void stack_dump(stack_t *stack) {
  $log("----------------Stack dump--------------------------\n");
  if (stack == NULL) {
    $log("Attempt to dump null ptr as a stack\n");
    return;
  }

  ON_STRUCT_HASH( uint64_t struct_hash = stack_struct_hash(*stack); )
  ON_BUF_HASH(
      uint64_t buf_hash = 0;

      if (stack->buf != NULL) {
        buf_hash = stack_buf_hash(stack);
      }
  )

  ON_DEBUG($log(
                   "stack_t " BLUE "\"%s\" " GREY "[%p]" RESET
                   " created by " GREEN "%s()" RESET " at " GREEN "%s:%d" RESET
                   "\n",
                   stack->name, stack, stack->init_func_name, stack->filename,
                   stack->line);)

  // clang-format off
  $log(

      ON_CANARY(       "CANARY            =" BLUE " 0x%016lx\n"
                      "" RESET "canary_begin      = " BLUE "0x%016lx\n")

                      "" RESET "buf               = " BLUE "[%p]\n"
                      "" RESET "size              = " BLUE "%zu\n"
                      "" RESET "capacity          = " BLUE "%zu\n" 

      ON_CANARY(
                      "" RESET "canary_end        = " BLUE "0x%016lx\n"
      )
       ON_STRUCT_HASH("" RESET "saved_struct_hash = " BLUE "0x%016lx\n")
       ON_BUF_HASH(   "" RESET "saved_buf_hash    = " BLUE "0x%016lx\n\n")
       ON_STRUCT_HASH("" RESET "cur_struct_hash   = " BLUE "0x%016lx\n")
       ON_BUF_HASH(   "" RESET "cur_buf_hash      = " BLUE "0x%016lx\n"),

      ON_CANARY(
                      *(unsigned long *)(&canary),
                      *(unsigned long *)(&stack->canary_begin),
      )
                      stack->buf,
                      stack->size,
                      stack->capacity
      ON_CANARY(
                     ,*(unsigned long *)(&stack->canary_end)
      )
      ON_STRUCT_HASH(,stack->struct_hash)
      ON_BUF_HASH(   ,stack->buf_hash)
      ON_STRUCT_HASH(,struct_hash)
      ON_BUF_HASH(   ,buf_hash));
  // clang-format on

  if (stack->buf != NULL) {
    ON_BUF_HASH(uint64_t buf_hash = stack_buf_hash(stack);)
    $log("\nStack data " GREY "[%p]" RESET ":\n", stack->buf);

    ON_CANARY(
    $log(
        MAGENTA "%p <- BUF CANARY BEGIN" RESET,
        *(stack->buf - 1));
          )

    for (int i = 0; i < stack->capacity; i += 1) {
      if (i < stack->size) {
        $log(BOLD CYAN "\n" GREEN "*[%d]" RESET "\t = ", i);
      } else {
        $log("\n " CYAN "[%d]" RESET "\t = ", i);
      }
      $log(BYELLOW ELEM_T_FORMAT_STRING RESET, stack->buf[i]);
    }

    ON_CANARY(
    $log(
        MAGENTA "\n%p <- BUF CANARY END\n" RESET,
        *(stack->buf + stack->capacity));
          )

  } else {
    $log("\nbuf: <nullptr>\n");
  }

  $log("\n----------------------------------------------------\n");
}

#ifdef STACK_DEBUG
void debug_stack_status_handler(stack_t *stack,
    stack_status_code status_code,
    const char *func_name,
    const char *filename,
    int line) {
  if (status_code != STACK_OK) {
    $error("Stack check failed in " GREEN "%s()" RESET " at " GREEN
           "%s:%d" RESET "\n",
        func_name,
        filename,
        line);

    switch (status_code) {

    case BUF_NULL_PTR:
      $error("Stack buffer is nullptr\n");
      break;

    case STACK_NULL_PTR:
      $error("Stack is NULL\n");
      break;

    case SIZE_GREATER_THAN_CAP:
      $error("Stack capacity < size\n");
      break;

    case ALLOCATED_NOT_ENOUGH:
      $error("Allocated less memory than capacity require\n");
      break;

    ON_CANARY(
    case STRUCT_CANARY_BEGIN_MISMATCH:
      $error("Canary in the beginning of structure is corrupted\n");
      break;

    case STRUCT_CANARY_END_MISMATCH:
      $error("Canary in the end of structure is corrupted\n");
      break;

    case BUF_CANARY_BEGIN_MISMATCH:
      $error("Canary in the beggining of buffer is corrupted\n");
      break;

    case BUF_CANARY_END_MISMATCH:
      $error("Canary in the end of buffer is corrupted\n");
      break;
          )

    ON_STRUCT_HASH(
    case STRUCT_HASH_MISMATCH:
      $error("The structure hash does not match the stored one.\n");
      break;
    )

    ON_BUF_HASH(
    case BUF_HASH_MISMATCH:
      $error("The buffer hash does not match the stored one.\n");
      break;
    )

    case STACK_OK:
      break;
    }

    stack_dump(stack);

    fprintf(stderr,
        "Error: Press q to abort program, or any other key to continue\n");
    char a = getchar();
    if (a == 'q') {
      abort();
    }
  } else {
    $info("Stack check ok in " GREEN "%s()" RESET " at " GREEN "%s:%d" RESET
          "\n" RED,
        func_name,
        filename,
        line);
  }
}

#define STACK_CHECK(stack)                                                     \
  debug_stack_status_handler(                                                  \
      stack, verify(stack), __func__, __FILE__, __LINE__);
#else
#define STACK_CHECK(stack) assert(verify(stack) == STACK_OK);
#endif

ON_CANARY(
    inline void place_buf_canaries(elem_t *buf, size_t capacity) {
      *(buf - 1)        = canary;
      *(buf + capacity) = canary;
    }
)

// ------------------------------------Stack_init_function---------------------------------------

void stack_init(stack_t *stack,
    size_t capacity

    ON_DEBUG(, const char *filename, const char *name,
                         const char *init_func_name, const int line)

) {
  assert(stack != NULL);

  ON_DEBUG(
      stack->filename       = filename;
      stack->name           = name;
      stack->init_func_name = init_func_name;
      stack->line           = line;)

  ON_CANARY(
      if (is_canary_set == false) {
        is_canary_set = true;
        srand(time(NULL));
        // Random generation to guarantee that canary is not 0.
        unsigned char *iter = (unsigned char *)&canary;
        for (int i = 0; i < sizeof(elem_t); i += 1) {
          *(iter + i) = (((long)rand() & 0b11111110) + ((rand() % 9) + 1));
        }
      }
  )

  if (capacity < STACK_MIN_CAPACITY) {
    capacity = STACK_MIN_CAPACITY;
  }

  stack->capacity = capacity;
  stack->buf      = calloc_stack_buf(capacity);
  stack->size     = 0;

  ON_CANARY(
      stack->canary_begin = canary;
      stack->canary_end   = canary;

      place_buf_canaries(stack->buf, capacity);
  )

#if defined(STACK_BUF_HASH_CHECK) || defined(STACK_STRUCT_HASH_CHECK)
  update_hashes(stack);
#endif

  STACK_CHECK(stack);
}

// ------------------------------------Stack_init_function_end---------------------------------------

void stack_destruct(stack_t *stack) {
  STACK_CHECK(stack)
  free(stack->buf ON_CANARY(- 1));
}

void stack_resize(stack_t *stack, size_t new_capacity) {
  STACK_CHECK(stack)

  if (new_capacity < stack->size) {
    new_capacity = stack->size;
  }
  if (new_capacity < STACK_MIN_CAPACITY) {
    new_capacity = STACK_MIN_CAPACITY;
  }

  stack->capacity = new_capacity;

  stack->buf = realloc_stack_buf(stack->buf, new_capacity);

  ON_CANARY(
  place_buf_canaries(stack->buf, new_capacity);
  )

#if defined(STACK_BUF_HASH_CHECK) || defined(STACK_STRUCT_HASH_CHECK)
  update_hashes(stack);
#endif

  STACK_CHECK(stack);
}

void stack_push(stack_t *stack, elem_t elem) {
  STACK_CHECK(stack)

  if (stack->size == stack->capacity) {
    stack_resize(stack, stack->capacity * 2);
  }

  stack->buf[stack->size] = elem;
  stack->size += 1;

#if defined(STACK_BUF_HASH_CHECK) || defined(STACK_STRUCT_HASH_CHECK)
  update_hashes(stack);
#endif

  STACK_CHECK(stack)
}

elem_t stack_pop(stack_t *stack) {
  STACK_CHECK(stack)

  assert(stack->size > 0);
  elem_t ret = stack->buf[stack->size - 1];
  stack->size -= 1;

  if (stack->size * SHRINK_RATIO < stack->capacity &&
      stack->size > STACK_MIN_CAPACITY) {
    stack_resize(stack, stack->size);
  }

#if defined(STACK_BUF_HASH_CHECK) || defined(STACK_STRUCT_HASH_CHECK)
  update_hashes(stack);
#endif

  STACK_CHECK(stack)
  return ret;
}
