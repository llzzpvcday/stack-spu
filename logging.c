#include "colors.h"
#include <stdio.h>
#include <time.h>

// ------------Log-level-Settings----------------------------

// #define LOG_LEVEL_ERROR
// #define LOG_LEVEL_INFO
#define LOG_LEVEL_DEBUG

// ----------------------------------------------------------

// ------------Log-level-defines-----------------------------

#ifdef LOG_LEVEL_DEBUG
#define LOG_DEBUG
#define LOG_ERROR
#define LOG_INFO
#endif

#ifdef LOG_LEVEL_INFO
#define LOG_ERROR
#define LOG_INFO
#endif

#ifdef LOG_LEVEL_ERROR
#define LOG_ERROR
#endif

#if defined(LOG_INFO) | defined(LOG_ERROR) | defined(LOG_DEBUG)
#define LOG_ANY
#endif

// ----------------------------------------------------------

// ------------Functions-------------------------------------

FILE *log_file = NULL;

void get_cur_time_str(char *buf, size_t size, const char *format) {
  time_t now   = time(NULL);
  struct tm *t = localtime(&now);
  strftime(buf, size, format, t);
}

void log_init() {
  // Source - https://stackoverflow.com/a/10917605
  // Posted by smichak, modified by community. See post 'Timeline' for change
  // history Retrieved 2026-10-03, License - CC BY-SA 3.0

#ifdef LOG_ANY
  char filename[32];

  get_cur_time_str(filename, sizeof(filename), "%d-%m-%Y-%H:%M.log");
  printf("Log file: %s\n", filename);
  log_file = fopen(filename, "w");
  setvbuf(log_file, NULL, _IONBF, 0);
#endif
}

void log_close() {

#ifdef LOG_ANY
  fclose(log_file);
#endif
}

// ----------------------------------------------------------

// ------------Macro-defines---------------------------------

#ifdef LOG_DEBUG
#define $log(...)                                                              \
  {                                                                            \
    fprintf(log_file, __VA_ARGS__);                                            \
    fprintf(stderr, __VA_ARGS__);                                              \
  }
#else
#define $log(...)
#endif

#ifdef LOG_ANY
#define $log_with_level(color, level, ...)                                     \
  {                                                                            \
    char buffer[20];                                                           \
    get_cur_time_str(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S");             \
    fprintf(log_file, color "%19s - [" level "] - " RESET, buffer);            \
    fprintf(log_file, __VA_ARGS__);                                            \
    fprintf(log_file, RESET);                                                  \
    fprintf(stderr, color "%19s - [" level "] - " RESET, buffer);              \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, RESET);                                                    \
  }

#endif

#ifdef LOG_ERROR
#define $error(...) $log_with_level(RED, "ERROR", __VA_ARGS__);
#else
#define $error(...)
#endif

#ifdef LOG_INFO
#define $info(...) $log_with_level(GREEN, "INFO ", __VA_ARGS__);
#else
#define $info(...)
#endif

// ----------------------------------------------------------
