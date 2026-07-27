#include <protocol.h>
#include <stdlib.h>
#include <crc32intrin.h>

#define BASE_PACKET_LENGTH (4 + 3 + 2 + 1 + 1 + 4)

char *usbadc_protocol_encode_packet(struct USBADC_PROTOCOL_PACKET packet) {
  uint32_t length = BASE_PACKET_LENGTH + packet.length;
  char *data = (char *) malloc(sizeof(char) * length);

  char *ptr = data;

  // Magic bytes
  (*(ptr++)) = 0xAA;
  (*(ptr++)) = 0x55;
  (*(ptr++)) = 0xAA;
  (*(ptr++)) = 0x55;

  // Semantic Version
  (*(ptr++)) = 0;
  (*(ptr++)) = 0;
  (*(ptr++)) = 1;

  // Message ID
  (*(ptr++)) = packet.id >> 8;
  (*(ptr++)) = packet.id >> 0;

  // Packet Type
  (*(ptr++)) = packet.type;

  // Data Length
  (*(ptr++)) = packet.length;

  for (uint32_t i = 0 ; i < packet.length ; i++) {
    (*(ptr++)) = packet.data[i];
  }

  const uint32_t checksum = crc32(data, length - 4);

  (*(ptr++)) = checksum >> 24;
  (*(ptr++)) = checksum >> 16;
  (*(ptr++)) = checksum >>  8;
  (*(ptr++)) = checksum >>  0;

  return data;
}

bool usbadc_protocol_decode_packet(char *data, uint32_t length, struct USBADC_PROTOCOL_PACKET *out) {
  if (length < BASE_PACKET_LENGTH) return false;
  char *ptr = data;

  struct USBADC_PROTOCOL_PACKET out_packet = {0};

  // Magic bytes
  if ((*(ptr++)) != 0xAA) return false;
  if ((*(ptr++)) != 0x55) return false;
  if ((*(ptr++)) != 0xAA) return false;
  if ((*(ptr++)) != 0x55) return false;

  // Semantic Version
  if ((*(ptr++)) != USBADC_PROTOCOL_VERSION_MAJOR) return false;
  if ((*(ptr++)) != USBADC_PROTOCOL_VERSION_MINOR) return false;
  if ((*(ptr++)) != USBADC_PROTOCOL_VERSION_PATCH) return false;

  out_packet.id = 0;
  // Message ID
  out_packet.id += (*(ptr++));
  out_packet.id <<= 8;
  out_packet.id += (*(ptr++));

  out_packet.type = (*(ptr++));

  // Data Length
  out_packet.length = (*(ptr++));

  if (BASE_PACKET_LENGTH + out_packet.length != length) return false;

  out_packet.data = ptr;
  ptr += out_packet.length;

  const uint32_t checksum = crc32(data, length - 4);

  if ((uint8_t) (checksum >> 24) != (*(ptr++))) return false;
  if ((uint8_t) (checksum >> 16) != (*(ptr++))) return false;
  if ((uint8_t) (checksum >>  8) != (*(ptr++))) return false;
  if ((uint8_t) (checksum >>  0) != (*(ptr++))) return false;

  *out = out_packet;
  return true;
}
