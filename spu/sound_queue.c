#include <stdlib.h>
typedef struct sound {
  double freq;
  double dur;
  double start;
} sound;

typedef struct sound_queue {
  sound *buf;
  size_t buf_size;
  size_t size;
  sound *head;
  sound *tail;
} sound_queue;

void init_sound_queue(sound_queue *queue, size_t buf_size) {
  queue->buf_size = buf_size;
  queue->buf      = (sound *)calloc(buf_size, sizeof(sound));
  queue->size     = 0;
  queue->head     = queue->buf + buf_size - 1;
  queue->tail     = queue->buf + buf_size - 1;
}

void destruct_sound_queue(sound_queue *queue) { free(queue->buf); }

int put_sound(sound_queue *queue, sound s) {
  if (queue->size >= queue->buf_size)
    return -1;

  queue->size += 1;
  *queue->head = s;
  queue->head -= 1;
  if (queue->head < queue->buf) {
    queue->head = queue->buf + queue->buf_size - 1;
  }

  return 0;
}

int pop_sound(sound_queue *queue, sound *ret) {
  if (queue->size <= 0)
    return -1;

  *ret = *queue->tail;
  queue->tail -= 1;
  queue->size -= 1;
  if (queue->tail < queue->buf) {
    queue->tail = queue->buf + queue->buf_size - 1;
  }

  return 0;
}

int top_sound(sound_queue *queue, sound *ret) {
  if (queue->size <= 0)
    return -1;

  *ret = *queue->tail;
  return 0;
}
