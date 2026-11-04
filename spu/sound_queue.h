#include <stddef.h>

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

void init_sound_queue(sound_queue *queue, size_t buf_size);
void destruct_sound_queue(sound_queue *queue);
int put_sound(sound_queue *queue, sound s);
int pop_sound(sound_queue *queue, sound *ret);
int top_sound(sound_queue *queue, sound *ret);
