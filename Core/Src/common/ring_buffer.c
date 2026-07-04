#include "common/ring_buffer.h"
#include <string.h>

#define RING_BUF_EMPTY (0U)

void ring_buffer_push(struct ring_buffer *rb, const void *data)
{
    // {} head = 0, tail = 0
    // {1} head = 1, tail = 0
    // {1, 2} head = 2, tail = 0
    // {1, 2, 3} head = 3, tail = 0
    // {1, 2, 3, 4} head = 4, tail = 0
    if (ring_buffer_full(rb))
        ring_buffer_pop(rb, NULL);
    // buf size is encoded as an array of uint8_t values with size: buf_cap * buf_elem_size
    memcpy(&rb->buf[rb->head * rb->elem_size], data, rb->elem_size);

    rb->head++;
    if (rb->head >= rb->buf_size)
        rb->head = 0;
}

void ring_buffer_pop(struct ring_buffer *rb, void *data)
{
    // ASSERT(!ring_buffer_empty(rb));
    if (data)
        memcpy(data, &rb->buf[rb->tail * rb->elem_size], rb->elem_size);

    rb->tail++;
    if (rb->tail >= rb->buf_size)
        rb->tail = 0;
}

void ring_buffer_peek_head(const struct ring_buffer *rb, void *data, uint8_t offset)
{
    // head will be one index ahead of where last element was pushed
    int16_t offset_idx = ((int16_t)rb->head - 1) - offset;
    if (offset_idx < 0)
        offset_idx = rb->buf_size + offset_idx;

    memcpy(data, &rb->buf[offset_idx * rb->elem_size], rb->elem_size);
}

void ring_buffer_peek_tail(const struct ring_buffer *rb, void *data)
{
    if (ring_buffer_empty(rb))
        return;
    memcpy(data, &rb->buf[rb->tail * rb->elem_size], rb->elem_size);
}

bool ring_buffer_full(const struct ring_buffer *rb)
{
    uint8_t idx_after_head = rb->head + 1;
    if (idx_after_head >= rb->buf_size)
        idx_after_head = 0;
    return idx_after_head == rb->tail;
}

uint8_t ring_buffer_count(const struct ring_buffer *rb)
{
    if (ring_buffer_full(rb))
        return rb->buf_size;
    else if (rb->tail <= rb->head)
        return rb->head - rb->tail;

    // head = 0, tail = 3, buf_size = 5

    return (rb->buf_size - rb->tail) + rb->head;
}

bool ring_buffer_empty(const struct ring_buffer *rb)
{
    return rb->tail == rb->head;
}
