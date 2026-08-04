#ifndef BYTE_VECTOR_H
#define BYTE_VECTOR_H

#include <stdint.h>
#include <stdbool.h>

// Struct that represents a vector of bytes
// The vector doesn't support growth to prevent fragmenting memory
// on the MCU.
typedef struct byte_vector {
  char *data;
  uint32_t length; // Amount of bytes currently in the vector
  uint32_t capacity; // Total capacity of the vector
} byte_vector;

// Initializes a byte vector with a provided buffer and length.
byte_vector byte_vector_init(char *buffer, uint32_t buffer_length);
// Appends a single byte at the end of the vector
bool byte_vector_append(byte_vector *vector, char character);
// Appends a slice of bytes at the end of the vector
bool byte_vector_append_buffer(byte_vector *vector, const char *buffer, uint32_t buffer_length);
// Deletes the first n characters of the buffer
bool byte_vector_delete_first_n_chars(byte_vector *vector, uint32_t n);
// Returns a pointer to the free space remaining
uint32_t byte_vector_free_space_pointer(byte_vector *vector, char **out_buffer);

#endif //  BYTE_VECTOR_H
