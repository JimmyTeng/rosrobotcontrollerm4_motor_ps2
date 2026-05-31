#ifndef __COBS_H_
#define __COBS_H_

#include <stddef.h>
#include <stdint.h>

size_t cobs_encode(const uint8_t *input, size_t length, uint8_t *output);
size_t cobs_decode(const uint8_t *input, size_t length, uint8_t *output);

#endif
