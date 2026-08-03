#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// The protocol works by sending messages accross the serial line.
// A client to server or server to client is defined in this order:
// All types are sent in the Big-Endian order. Structs' layouts are defined by
// their layout in the C standard.
//
// magic bytes 0xAA 0x55 0xAA 0x55
// Length (u8) of the rest of the message.
// Version struct {u8, u8, u8} [Semantic Version](https://semver.org/) of the protocol.
// Checksum (u32)
// Identifier (u16) of the message. These can be reused and are meant for identification in the short term
// Type (u8) of the message.
// Bytes rest of the message according to the type

// +-------------+-------------+------------------+------------------------------------------+
// | Bytes       | Name        | Type             | Description                              |
// +-------------+-------------+------------------+------------------------------------------+
// | 0-3         | Magic       | u8[4]            | 0xAA 0x55 0xAA 0x55                      |
// | 7-8         | ID          | u16              | Request/response identifier.             |
// | 9           | Type        | u8               | Message type.                            |
// | 10          | Data Length | u8               | Bytes following this field.              |
// | 11..10+N    | Data        | u8[]             | Type-specific payload.                   |
// | 11+N..14+N  | CRC         | u32              | Packet checksum using CRC32.             |
// +-------------+-------------+------------------+------------------------------------------+

enum {
  // Server request to client
  USBADC_PROTOCOL_REQUEST_PING = 0x00,
  USBADC_PROTOCOL_REQUEST_READ_ADC = 0x01,
  USBADC_PROTOCOL_REQUEST_WRITE_PIN = 0x02,
  USBADC_PROTOCOL_REQUEST_REBOOT = 0x03,
  USBADC_PROTOCOL_REQUEST_VERSION = 0x04,

  // Client response to server
  USBADC_PROTOCOL_RESPONSE_PONG = 0x80,
  USBADC_PROTOCOL_RESPONSE_READ_ADC = 0x81,
  USBADC_PROTOCOL_RESPONSE_WRITE_PIN = 0x82,
  USBADC_PROTOCOL_RESPONSE_ERROR = 0x83,
  USBADC_PROTOCOL_RESPONSE_VERSION = 0x84,
};

#define USBADC_PROTOCOL_VERSION_MAJOR (0)
#define USBADC_PROTOCOL_VERSION_MINOR (0)
#define USBADC_PROTOCOL_VERSION_PATCH (1)


struct USBADC_PROTOCOL_REQUEST_PING {
  uint32_t bytes;
};

struct USBADC_PROTOCOL_REQUEST_READ_ADC {
  uint8_t pin;
};

struct USBADC_PROTOCOL_REQUEST_WRITE_PIN {
  uint8_t pin;
  uint8_t value; // Range [0; 255] maps to [0; 3.3V]
};

struct USBADC_PROTOCOL_REQUEST_VERSION {};

struct USBADC_PROTOCOL_RESPONSE_PONG {
  uint32_t bytes;
};

struct USBADC_PROTOCOL_RESPONSE_READ_ADC {
  uint32_t pin;
  uint16_t data;
};

struct USBADC_PROTOCOL_RESPONSE_WRITE_PIN {
  uint32_t pin;
  uint16_t data;
};

struct USBADC_PROTOCOL_RESPONSE_ERROR {
  uint8_t reason;
};
enum USBADC_PROTOCOL_RESPONSE_ERROR_REASONS {
  USBADC_PROTOCOL_RESPONSE_ERROR_REASON_NOT_AVAILABLE,
  USBADC_PROTOCOL_RESPONSE_ERROR_REASON_TIMEOUT,
  USBADC_PROTOCOL_RESPONSE_ERROR_REASON_UNKNOWN,
};

struct USBADC_PROTOCOL_RESPONSE_VERSION {
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
};

struct USBADC_PROTOCOL_PACKET {
  uint16_t id;
  uint8_t type;
  uint8_t length;
  char *data;
};

// Caller owns the returned memory.
uint32_t usbadc_protocol_encode_packet(struct USBADC_PROTOCOL_PACKET packet, char **out_bytes);

enum USBADC_PROTOCOL_DECODE_ERRORS {
    USBADC_PROTOCOL_DECODE_DATA_LENGTH_TOO_SMALL = -1,
    USBADC_PROTOCOL_DECODE_WRONG_MAGIC_BYTES     = -2,
    USBADC_PROTOCOL_DECODE_WRONG_CHECKSUM        = -3,
};
int32_t usbadc_protocol_decode_packet(char *data, uint32_t length, struct USBADC_PROTOCOL_PACKET *out);


#endif // PROTOCOL_H
