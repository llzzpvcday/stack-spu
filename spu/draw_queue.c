#include "draw_queue.h"
#include <stdlib.h>

void init_draw_queue(draw_queue *queue, size_t size) {
  queue->buf = (pixel_to_draw *)calloc(size, sizeof(pixel_to_draw));
  queue->offset = 0;
}

void destruct_draw_queue(draw_queue *queue) {
  free(queue->buf);
  queue->buf = NULL;
  queue->offset = -1;
}

void add_pixel_to_draw(draw_queue *queue, pixel_to_draw pixel) {
  queue->buf[queue->offset] = pixel;
  queue->offset += 1;
}


