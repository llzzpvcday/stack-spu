#include <stdint.h>
#include <stddef.h>

typedef struct pixel_to_draw {
  uint16_t y;
  uint16_t x;
  uint8_t r;
  uint8_t g;
  uint8_t b;
} pixel_to_draw;

typedef struct draw_queue {
  pixel_to_draw *buf;
  size_t offset;
} draw_queue;

void init_draw_queue(draw_queue *queue, size_t size) ;
void destruct_draw_queue(draw_queue *queue) ;
void add_pixel_to_draw(draw_queue *queue, pixel_to_draw pixel) ;
