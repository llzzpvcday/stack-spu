#include "logging.c"

int main() {
  log_init();
  $info("Hello, world!");
  log_close();
}
