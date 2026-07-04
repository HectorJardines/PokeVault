#ifndef _RING_BUFFER_H
#define _RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

struct ring_buffer
{
    uint8_t *buf;
    uint8_t buf_size;
    uint8_t elem_size;
    uint8_t head;
    uint8_t tail;
};

#define RING_BUFFER(name, size, type, storage)                                                     \
    static_assert(size < UINT8_MAX);                                                               \
    storage uint8_t name##_buffer[size * sizeof(type)] = { 0 };                                    \
    storage struct ring_buffer name = { .buf = name##_buffer,                                   \
                                        .buf_size = size,                                       \
                                        .elem_size = sizeof(type) }

#define STATIC_RING_BUFFER(name, size, type) RING_BUFFER(name, size, type, static)

void ring_buffer_push(struct ring_buffer *rb, const void *data);
void ring_buffer_pop(struct ring_buffer *rb, void *data);
void ring_buffer_peek_tail(const struct ring_buffer *rb, void *data);
void ring_buffer_peek_head(const struct ring_buffer *rb, void *data, uint8_t offset);
bool ring_buffer_full(const struct ring_buffer *rb);
bool ring_buffer_empty(const struct ring_buffer *rb);
uint8_t ring_buffer_count(const struct ring_buffer *rb);

#endif