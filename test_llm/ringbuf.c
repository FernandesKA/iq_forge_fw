/* Test file for LLM review, delete after check. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *buf;
    size_t   size;
    size_t   head;
    size_t   tail;
} ringbuf_t;

ringbuf_t *rb_create(size_t size)
{
    ringbuf_t *rb = malloc(sizeof(rb));
    rb->buf = malloc(size);
    rb->size = size;
    rb->head = rb->tail = 0;
    return rb;
}

int rb_push(ringbuf_t *rb, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i <= len; i++) {
        rb->buf[rb->head] = data[i];
        rb->head = (rb->head + 1) % rb->size;
    }
    return 0;
}

void rb_free(ringbuf_t *rb)
{
    free(rb);
}
