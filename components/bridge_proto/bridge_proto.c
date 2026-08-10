#include "bridge_proto.h"
#include <string.h>

uint8_t bridge_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ BRIDGE_CRC8_POLY);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

void bridge_encode(const bridge_frame_t *frame, uint8_t out[BRIDGE_FRAME_SIZE])
{
    out[0] = BRIDGE_SYNC;
    out[1] = frame->type;
    out[2] = frame->seq;
    memcpy(&out[3], frame->payload, BRIDGE_PAYLOAD_SIZE);
    out[BRIDGE_FRAME_SIZE - 1] = bridge_crc8(out, BRIDGE_FRAME_SIZE - 1);
}

bool bridge_decode(const uint8_t in[BRIDGE_FRAME_SIZE], bridge_frame_t *frame)
{
    if (in[0] != BRIDGE_SYNC) {
        return false;
    }
    if (bridge_crc8(in, BRIDGE_FRAME_SIZE - 1) != in[BRIDGE_FRAME_SIZE - 1]) {
        return false;
    }
    frame->type = in[1];
    frame->seq = in[2];
    memcpy(frame->payload, &in[3], BRIDGE_PAYLOAD_SIZE);
    return true;
}

bool bridge_type_known(uint8_t type)
{
    switch (type) {
        case BRIDGE_TYPE_IDLE:
        case BRIDGE_TYPE_KEY_REPORT:
        case BRIDGE_TYPE_LED_REPORT:
        case BRIDGE_TYPE_HEARTBEAT:
            return true;
        default:
            return false;
    }
}
