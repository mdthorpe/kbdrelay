#ifndef BRIDGE_PROTO_H
#define BRIDGE_PROTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fixed-size 12-byte SPI frame (plan.md "SPI Protocol"):
 *
 *   Byte 0     SYNC   (0xA5)
 *   Byte 1     TYPE
 *   Byte 2     SEQ    (wrapping counter, debug aid)
 *   Bytes 3-10 PAYLOAD (8 bytes, zero-padded)
 *   Byte 11    CRC8   (poly 0x07 over bytes 0-10)
 */

#define BRIDGE_FRAME_SIZE   12
#define BRIDGE_PAYLOAD_SIZE 8
#define BRIDGE_SYNC         0xA5u
#define BRIDGE_CRC8_POLY    0x07u

/* Message types (FR2.3). New types append without breaking old ones. */
typedef enum {
    BRIDGE_TYPE_IDLE       = 0x00, /* nothing to say */
    BRIDGE_TYPE_KEY_REPORT = 0x01, /* KBD -> TGT: 8-byte boot keyboard report */
    BRIDGE_TYPE_LED_REPORT = 0x02, /* TGT -> KBD: payload[0] = HID LED bitmap */
    BRIDGE_TYPE_HEARTBEAT  = 0x03, /* KBD -> TGT: liveness */
} bridge_type_t;

typedef struct {
    uint8_t type;
    uint8_t seq;
    uint8_t payload[BRIDGE_PAYLOAD_SIZE];
} bridge_frame_t;

/* CRC-8 (poly 0x07, init 0x00, MSB-first, no reflection). */
uint8_t bridge_crc8(const uint8_t *data, size_t len);

/* Serialize frame into exactly BRIDGE_FRAME_SIZE bytes (sets SYNC + CRC). */
void bridge_encode(const bridge_frame_t *frame, uint8_t out[BRIDGE_FRAME_SIZE]);

/*
 * Parse BRIDGE_FRAME_SIZE bytes. Returns true only if SYNC and CRC are valid
 * (FR2.4: corrupt frames are rejected). Unknown TYPE values are tolerated and
 * passed through unchanged (FR2.3 forward compatibility).
 */
bool bridge_decode(const uint8_t in[BRIDGE_FRAME_SIZE], bridge_frame_t *frame);

/* True if type is one this build knows how to act on. */
bool bridge_type_known(uint8_t type);

#ifdef __cplusplus
}
#endif

#endif /* BRIDGE_PROTO_H */
