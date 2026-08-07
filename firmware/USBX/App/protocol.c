#include "stm32u073xx.h"
#include <protocol.h>
#include <stm32u0xx_hal.h>
#include <stdlib.h>

// TODO: Check how to use the hardware CRC32 calculator in byte mode 
// instead of using this for a minor speedup
static uint32_t crc32(const void *data, size_t length)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;

    while (length--) {
        crc ^= *p++;

        for (int i = 0; i < 8; i++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

#define BASE_PACKET_LENGTH (4 + 2 + 1 + 1 + 4)

uint32_t usbadc_protocol_encode_packet(struct USBADC_PROTOCOL_PACKET packet, char **out_bytes) {
  const uint32_t length = BASE_PACKET_LENGTH + packet.length;
  char *data = (char *) malloc(sizeof(char) * length);
  if (data == NULL) return 0;

  char *ptr = data;

  // Magic bytes
  (*(ptr++)) = 0xAA;
  (*(ptr++)) = 0x55;
  (*(ptr++)) = 0xAA;
  (*(ptr++)) = 0x55;

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

  *out_bytes = data;
  return length;
}

int32_t usbadc_protocol_decode_packet(char *data, uint32_t length, struct USBADC_PROTOCOL_PACKET *out) {
  if (length < BASE_PACKET_LENGTH) return USBADC_PROTOCOL_DECODE_DATA_LENGTH_TOO_SMALL;
  char *ptr = data;

  struct USBADC_PROTOCOL_PACKET out_packet = {0};

  // Magic bytes
  if ((*(ptr++)) != 0xAA) return USBADC_PROTOCOL_DECODE_WRONG_MAGIC_BYTES;
  if ((*(ptr++)) != 0x55) return USBADC_PROTOCOL_DECODE_WRONG_MAGIC_BYTES;
  if ((*(ptr++)) != 0xAA) return USBADC_PROTOCOL_DECODE_WRONG_MAGIC_BYTES;
  if ((*(ptr++)) != 0x55) return USBADC_PROTOCOL_DECODE_WRONG_MAGIC_BYTES;

  out_packet.id = 0;
  // Message ID
  out_packet.id += (*(ptr++));
  out_packet.id <<= 8;
  out_packet.id += (*(ptr++));

  out_packet.type = (*(ptr++));

  // Data Length
  out_packet.length = (*(ptr++));

  if (BASE_PACKET_LENGTH + out_packet.length > length) return USBADC_PROTOCOL_DECODE_DATA_LENGTH_TOO_SMALL;

  out_packet.data = ptr;
  ptr += out_packet.length;

  const uint32_t checksum = crc32(data, BASE_PACKET_LENGTH + out_packet.length - 4);

  if ((uint8_t) (checksum >> 24) != (*(ptr++))) return USBADC_PROTOCOL_DECODE_WRONG_CHECKSUM;
  if ((uint8_t) (checksum >> 16) != (*(ptr++))) return USBADC_PROTOCOL_DECODE_WRONG_CHECKSUM;
  if ((uint8_t) (checksum >>  8) != (*(ptr++))) return USBADC_PROTOCOL_DECODE_WRONG_CHECKSUM;
  if ((uint8_t) (checksum >>  0) != (*(ptr++))) return USBADC_PROTOCOL_DECODE_WRONG_CHECKSUM;

  *out = out_packet;
  return BASE_PACKET_LENGTH + out_packet.length;
}
