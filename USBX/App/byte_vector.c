#include "byte_vector.h"
#include <string.h>

byte_vector byte_vector_init(char *buffer, uint32_t buffer_length) {
  return (byte_vector) {
    .data = buffer,
    .capacity = buffer_length,
    .length = 0,
  };
}

bool byte_vector_append(byte_vector *vector, char character) {
  if (vector->length >= vector->capacity) return false;

  vector->data[vector->length] = character;
  vector->length += 1;
  return true;
}

bool byte_vector_append_buffer(byte_vector *vector, const char *buffer, uint32_t buffer_length) {
  if (vector->length + buffer_length >= vector->capacity) return false;

  char *ptr = vector->data + vector->length;
  memmove(ptr, buffer, buffer_length);
  vector->length += buffer_length;
  return true;
}

bool byte_vector_delete_first_n_chars(byte_vector *vector, uint32_t n) {
  if (vector->length < n) return false;

  memmove(vector->data, vector->data + n, vector->length - n);
  vector->length -= n;
  return true;
}

uint32_t byte_vector_free_space_pointer(byte_vector *vector, char **out_buffer) {
  *out_buffer = vector->data + vector->length;
  return vector->capacity - vector->length;
}
