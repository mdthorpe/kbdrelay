#include <unity.h>
#include <string.h>
#include "bridge_proto.h"

void setUp(void) {}
void tearDown(void) {}

/* CRC-8/SMBUS (poly 0x07, init 0x00) of "123456789" is 0xF4 — sanity anchor. */
static void test_crc8_known_vector(void)
{
    const uint8_t v[] = {'1','2','3','4','5','6','7','8','9'};
    TEST_ASSERT_EQUAL_HEX8(0xF4, bridge_crc8(v, sizeof(v)));
}

static void test_roundtrip_key_report(void)
{
    bridge_frame_t in = { .type = BRIDGE_TYPE_KEY_REPORT, .seq = 42 };
    const uint8_t report[BRIDGE_PAYLOAD_SIZE] = {0x02,0x00,0x04,0x05,0x00,0x00,0x00,0x00};
    memcpy(in.payload, report, BRIDGE_PAYLOAD_SIZE);

    uint8_t wire[BRIDGE_FRAME_SIZE];
    bridge_encode(&in, wire);

    TEST_ASSERT_EQUAL_HEX8(BRIDGE_SYNC, wire[0]);

    bridge_frame_t out;
    TEST_ASSERT_TRUE(bridge_decode(wire, &out));
    TEST_ASSERT_EQUAL_HEX8(BRIDGE_TYPE_KEY_REPORT, out.type);
    TEST_ASSERT_EQUAL_UINT8(42, out.seq);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(report, out.payload, BRIDGE_PAYLOAD_SIZE);
}

static void test_roundtrip_all_types(void)
{
    const uint8_t types[] = {
        BRIDGE_TYPE_IDLE, BRIDGE_TYPE_KEY_REPORT,
        BRIDGE_TYPE_LED_REPORT, BRIDGE_TYPE_HEARTBEAT
    };
    for (size_t i = 0; i < sizeof(types); i++) {
        bridge_frame_t in = { .type = types[i], .seq = (uint8_t)i };
        uint8_t wire[BRIDGE_FRAME_SIZE];
        bridge_encode(&in, wire);
        bridge_frame_t out;
        TEST_ASSERT_TRUE(bridge_decode(wire, &out));
        TEST_ASSERT_EQUAL_HEX8(types[i], out.type);
    }
}

/* FR2.4: a corrupted payload byte must fail the CRC check. */
static void test_crc_rejection(void)
{
    bridge_frame_t in = { .type = BRIDGE_TYPE_LED_REPORT, .seq = 1 };
    in.payload[0] = 0x07;
    uint8_t wire[BRIDGE_FRAME_SIZE];
    bridge_encode(&in, wire);

    wire[5] ^= 0xFF; /* flip a payload bit -> CRC no longer matches */

    bridge_frame_t out;
    TEST_ASSERT_FALSE(bridge_decode(wire, &out));
}

/* FR2.4: a bad SYNC byte must be rejected. */
static void test_bad_sync_rejection(void)
{
    bridge_frame_t in = { .type = BRIDGE_TYPE_HEARTBEAT, .seq = 9 };
    uint8_t wire[BRIDGE_FRAME_SIZE];
    bridge_encode(&in, wire);

    wire[0] = 0x00; /* not SYNC */

    bridge_frame_t out;
    TEST_ASSERT_FALSE(bridge_decode(wire, &out));
}

/* FR2.3: unknown/future types decode fine (with valid framing) but report
 * as not-known, so a receiver can ignore them without breaking. */
static void test_unknown_type_tolerated(void)
{
    bridge_frame_t in = { .type = 0x7E, .seq = 3 };
    uint8_t wire[BRIDGE_FRAME_SIZE];
    bridge_encode(&in, wire);

    bridge_frame_t out;
    TEST_ASSERT_TRUE(bridge_decode(wire, &out));
    TEST_ASSERT_EQUAL_HEX8(0x7E, out.type);
    TEST_ASSERT_FALSE(bridge_type_known(out.type));
    TEST_ASSERT_TRUE(bridge_type_known(BRIDGE_TYPE_KEY_REPORT));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc8_known_vector);
    RUN_TEST(test_roundtrip_key_report);
    RUN_TEST(test_roundtrip_all_types);
    RUN_TEST(test_crc_rejection);
    RUN_TEST(test_bad_sync_rejection);
    RUN_TEST(test_unknown_type_tolerated);
    return UNITY_END();
}
