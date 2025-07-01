#pragma once

#include <stdint.h>
#include <stddef.h>
#include <ringbuffer.h>
#include <vfs.h>

#define EVDEV_BUF_SIZE 1024

typedef struct {
    uint16_t type;
    uint16_t code;
    int32_t value;
} input_event_t;

typedef struct {
    ringbuffer_t *ringbuf; // input_event_t
} evdev_t;

void evdev_init();

evdev_t *evdev_create(char *name);
