/*
 * TGT firmware — SPI slave + boot-keyboard replay (task T3).
 *
 * Presents a vanilla USB 1.1 boot keyboard to the target (FR3.1) and replays
 * KEY_REPORT frames received over SPI from KBD, verbatim (FR3.2). A watchdog
 * releases all keys if the link goes quiet, so keys never stick (FR4.1).
 *
 * SPI: slave, mode 0, fixed 12-byte bridge_proto frames. KBD is master.
 * Console on UART0 (USB-C is the keyboard the target sees).
 *
 * Status LED (onboard WS2812, RGB order):
 *   - link alive (recent valid frame) -> solid green
 *   - link quiet / watchdog            -> slow red blink
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "led_strip.h"
#include "bridge_proto.h"

static const char *TAG = "tgt";

#define WS2812_GPIO   21
#define PIN_SCK       4
#define PIN_MOSI      5
#define PIN_MISO      6
#define PIN_CS        7
#define PIN_DR        8            /* DATA_READY: output (LED backchannel, later) */
#define SPI_HOST_ID   SPI2_HOST
#define WATCHDOG_US   75000        /* FR4.1: release keys well within 100 ms */

/* ---- USB descriptors: single boot-protocol keyboard interface (FR3.1) --- */

static const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

enum { ITF_NUM_HID = 0, ITF_NUM_TOTAL };
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID 0x81

static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_KEYBOARD,
                       sizeof(hid_report_descriptor), EPNUM_HID, 8, 10),
};

static const tusb_desc_device_t device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0110,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A,
    .idProduct          = 0x4004,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

static const char *string_descriptor[] = {
    (const char[]){0x09, 0x04},
    "kbdrelay",
    "kbdrelay TGT keyboard",
    "000001",
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

/* Latest keyboard LED bitmap from the target host, pending forward to KBD. */
static volatile uint8_t s_led_state;
static volatile bool s_led_pending;

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    /* Host LED output report (Caps/Num/Scroll) -> queue it for KBD (FR2.2). */
    (void)instance; (void)report_id;
    if (report_type == HID_REPORT_TYPE_OUTPUT && bufsize >= 1) {
        s_led_state = buffer[0];
        s_led_pending = true;
        gpio_set_level(PIN_DR, 1);   /* tell KBD an LED report is waiting */
    }
}

/* ---- application ------------------------------------------------------- */

static led_strip_handle_t s_led;
static volatile int64_t s_last_valid_us;

static void led_task(void *arg)
{
    (void)arg;
    bool blink = false;
    while (1) {
        bool alive = (esp_timer_get_time() - s_last_valid_us) < WATCHDOG_US;
        if (alive) {
            led_strip_set_pixel(s_led, 0, 0, 20, 0);   /* solid green: link up */
            led_strip_refresh(s_led);
            vTaskDelay(pdMS_TO_TICKS(120));
        } else {
            blink = !blink;                             /* red blink: no link */
            if (blink) {
                led_strip_set_pixel(s_led, 0, 20, 0, 0);
            } else {
                led_strip_clear(s_led);
            }
            led_strip_refresh(s_led);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
    }
}

static bool send_hid(const uint8_t report8[8])
{
    if (tud_mounted() && tud_hid_ready()) {
        return tud_hid_report(0, report8, 8);
    }
    return false;
}

static void spi_task(void *arg)
{
    (void)arg;
    WORD_ALIGNED_ATTR static uint8_t txbuf[BRIDGE_FRAME_SIZE];
    WORD_ALIGNED_ATTR static uint8_t rxbuf[BRIDGE_FRAME_SIZE];

    /* Preload an IDLE frame as our TX (LED backchannel comes later). */
    bridge_frame_t idle = { .type = BRIDGE_TYPE_IDLE };
    bridge_encode(&idle, txbuf);

    spi_slave_transaction_t t = {
        .length = BRIDGE_FRAME_SIZE * 8,
        .tx_buffer = txbuf,
        .rx_buffer = rxbuf,
    };
    spi_slave_transaction_t *ret = NULL;
    ESP_ERROR_CHECK(spi_slave_queue_trans(SPI_HOST_ID, &t, portMAX_DELAY));

    bool keys_released = true;
    bool armed_led = false;   /* the currently-queued TX carries a LED_REPORT */
    const uint8_t zero8[8] = {0};

    while (1) {
        esp_err_t r = spi_slave_get_trans_result(SPI_HOST_ID, &ret, pdMS_TO_TICKS(20));
        if (r == ESP_OK) {
            bridge_frame_t f;
            if (bridge_decode(rxbuf, &f)) {
                s_last_valid_us = esp_timer_get_time();
                if (f.type == BRIDGE_TYPE_KEY_REPORT) {
                    send_hid(f.payload);          /* replay verbatim (FR3.2) */
                    keys_released = false;
                }
                /* IDLE / HEARTBEAT: liveness only. */
            }
            /* If the frame we just clocked out was the LED report, the master
             * has now received it — clear the pending state. */
            if (armed_led) {
                s_led_pending = false;
                armed_led = false;
                gpio_set_level(PIN_DR, 0);
            }
            /* Preload the next TX: a LED_REPORT if one is pending, else IDLE. */
            if (s_led_pending) {
                bridge_frame_t led = { .type = BRIDGE_TYPE_LED_REPORT };
                led.payload[0] = s_led_state;
                bridge_encode(&led, txbuf);
                armed_led = true;
            } else {
                bridge_encode(&idle, txbuf);
            }
            /* Re-arm immediately so the slave is ready for the next frame. */
            ESP_ERROR_CHECK(spi_slave_queue_trans(SPI_HOST_ID, &t, portMAX_DELAY));
        }

        /* Watchdog: link quiet -> release all keys (FR4.1). Retry every loop
         * until the empty report is actually accepted (endpoint may be busy),
         * so a single dropped send can't leave a key stuck. */
        if (!keys_released &&
            (esp_timer_get_time() - s_last_valid_us) > WATCHDOG_US) {
            if (send_hid(zero8)) {
                keys_released = true;
                ESP_LOGW(TAG, "watchdog: link quiet, released all keys");
            }
        }
    }
}

void app_main(void)
{
    /* DATA_READY output, deasserted (LED backchannel not implemented yet). */
    gpio_config_t dr_cfg = {
        .pin_bit_mask = 1ULL << PIN_DR,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&dr_cfg));
    gpio_set_level(PIN_DR, 0);

    led_strip_config_t strip_cfg = {
        .strip_gpio_num = WS2812_GPIO,
        .max_leds = 1,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
    };
    led_strip_rmt_config_t rmt_cfg = { .resolution_hz = 10 * 1000 * 1000 };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led));
    led_strip_clear(s_led);
    xTaskCreate(led_task, "led", 2048, NULL, 2, NULL);

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device            = &device_descriptor;
    tusb_cfg.descriptor.string            = string_descriptor;
    tusb_cfg.descriptor.string_count      = sizeof(string_descriptor) / sizeof(string_descriptor[0]);
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_slave_interface_config_t slvcfg = {
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 3,
        .flags = 0,
    };
    ESP_ERROR_CHECK(spi_slave_initialize(SPI_HOST_ID, &buscfg, &slvcfg, SPI_DMA_CH_AUTO));
    /* The idle-high pull-up for CS comes from TGT's OWN rail, so an unpowered
     * TGT presents 0 V here and KBD's open-drain CS cannot back-feed it.
     * ~45k internal => ~60 uA in the reverse case (TGT up, KBD down). */
    ESP_ERROR_CHECK(gpio_set_pull_mode(PIN_CS, GPIO_PULLUP_ONLY));

    xTaskCreate(spi_task, "spi", 4096, NULL, 6, NULL);

    ESP_LOGI(TAG, "TGT bridge up: SPI slave + boot-keyboard replay");
}
