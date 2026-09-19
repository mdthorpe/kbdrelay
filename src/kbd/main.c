/*
 * KBD firmware — USB host bring-up (subset of task T4).
 *
 * Enumerates a USB keyboard plugged into KBD's USB-C port, requests boot
 * protocol (FR1.3), supports keyboards that sit behind an internal hub
 * (FR1.2, via CONFIG_USB_HOST_HUBS_SUPPORTED), and logs each raw 8-byte boot
 * report over UART. Hot-plug is handled (FR1.4).
 *
 * Onboard WS2812 status LED:
 *   - powered, no keyboard  -> slow blue blink (~1 Hz)
 *   - keyboard connected     -> solid green
 *
 * No SPI / bridge_proto yet — this proves we can read the keyboard and see its
 * reports on the serial monitor. Forwarding those reports over SPI is next (T4).
 *
 * Console is on UART0 (TX/RX pads) because the USB-C port is the keyboard
 * host — view logs via a 3.3 V USB-UART adapter at 115200 baud.
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "led_strip.h"
#include "bridge_proto.h"

static const char *TAG = "kbd";

#define WS2812_GPIO 21

/* SPI master link to TGT (see plan.md wiring). */
#define PIN_SCK     4
#define PIN_MOSI    5
#define PIN_MISO    6
#define PIN_CS      7
#define PIN_DR      8            /* DATA_READY: input (LED backchannel, later) */
#define SPI_HOST_ID SPI2_HOST
#define SPI_CLOCK_HZ 250000
#define HEARTBEAT_MS 25

static spi_device_handle_t s_spi;
static QueueHandle_t s_report_queue;   /* items: 8-byte boot keyboard reports */

static QueueHandle_t s_event_queue;
static led_strip_handle_t s_led;

/* Handles of interfaces we've identified and started as keyboards. Used both
 * for the status LED (any present => green) and to distinguish a real keyboard
 * unplug from the DISCONNECTED event that fires when we close a non-keyboard
 * interface. Mutated from the app task (connect) and HID driver task
 * (disconnect); guarded by a spinlock. */
#define MAX_KB_IFACES 8
#define KB_REPORT_MAX 16

typedef struct {
    hid_host_device_handle_t handle;  /* NULL = free slot */
    uint8_t last[KB_REPORT_MAX];      /* previous report, for change detection */
    size_t last_len;
} kb_slot_t;

static kb_slot_t s_kb[MAX_KB_IFACES];
static volatile int s_kb_ifaces;
static portMUX_TYPE s_kb_lock = portMUX_INITIALIZER_UNLOCKED;

static void kb_track_add(hid_host_device_handle_t h)
{
    portENTER_CRITICAL(&s_kb_lock);
    for (int i = 0; i < MAX_KB_IFACES; i++) {
        if (s_kb[i].handle == NULL) {
            s_kb[i].handle = h;
            s_kb[i].last_len = 0;
            s_kb_ifaces++;
            break;
        }
    }
    portEXIT_CRITICAL(&s_kb_lock);
}

/* Returns true if the handle was a tracked keyboard (and removes it). */
static bool kb_track_remove(hid_host_device_handle_t h)
{
    bool found = false;
    portENTER_CRITICAL(&s_kb_lock);
    for (int i = 0; i < MAX_KB_IFACES; i++) {
        if (s_kb[i].handle == h) {
            s_kb[i].handle = NULL;
            s_kb_ifaces--;
            found = true;
            break;
        }
    }
    portEXIT_CRITICAL(&s_kb_lock);
    return found;
}

/* Copy the current tracked keyboard handles out (for use outside the lock). */
static int kb_snapshot_handles(hid_host_device_handle_t *out, int max)
{
    int c = 0;
    portENTER_CRITICAL(&s_kb_lock);
    for (int i = 0; i < MAX_KB_IFACES && c < max; i++) {
        if (s_kb[i].handle != NULL) {
            out[c++] = s_kb[i].handle;
        }
    }
    portEXIT_CRITICAL(&s_kb_lock);
    return c;
}

/* Deliver a keyboard LED bitmap (Caps/Num/Scroll) to every open keyboard
 * interface via HID Set Report (FR2.2). Deduped by the caller. */
static void kb_set_leds(uint8_t leds)
{
    hid_host_device_handle_t handles[MAX_KB_IFACES];
    int n = kb_snapshot_handles(handles, MAX_KB_IFACES);
    uint8_t buf[1] = { leds };
    for (int i = 0; i < n; i++) {
        esp_err_t e = hid_class_request_set_report(handles[i], HID_REPORT_TYPE_OUTPUT, 0, buf, 1);
        if (e != ESP_OK) {
            ESP_LOGW(TAG, "set LED report failed: %s", esp_err_to_name(e));
        }
    }
}

/* True if this report differs from the last one for this interface (updating
 * the stored copy). Collapses free-running streams to one event per change —
 * the bridge only cares about changes anyway (FR3.2). */
static bool kb_report_changed(hid_host_device_handle_t h, const uint8_t *data, size_t len)
{
    bool changed = true;
    portENTER_CRITICAL(&s_kb_lock);
    for (int i = 0; i < MAX_KB_IFACES; i++) {
        if (s_kb[i].handle == h) {
            size_t n = (len > KB_REPORT_MAX) ? KB_REPORT_MAX : len;
            if (s_kb[i].last_len == n && memcmp(s_kb[i].last, data, n) == 0) {
                changed = false;
            } else {
                memcpy(s_kb[i].last, data, n);
                s_kb[i].last_len = n;
            }
            break;
        }
    }
    portEXIT_CRITICAL(&s_kb_lock);
    return changed;
}

typedef struct {
    hid_host_device_handle_t handle;
    hid_host_driver_event_t event;
} app_event_t;

/*
 * Walk a HID report descriptor and report whether it declares a keyboard
 * (Usage Page = Generic Desktop 0x01, Usage = Keyboard 0x06 at a Collection).
 * This is more robust than trusting the interface protocol byte: composite /
 * non-boot keyboards report sub_class=0/proto=0 yet still carry a keyboard
 * usage here (FR1.2, plan.md non-boot-descriptor risk).
 */
static bool descriptor_is_keyboard(const uint8_t *d, size_t len)
{
    if (d == NULL) {
        return false;
    }
    uint16_t usage_page = 0;
    uint16_t usage = 0;
    size_t i = 0;
    while (i < len) {
        const uint8_t item = d[i++];
        uint8_t size = item & 0x03;          /* bSize code: 0,1,2,3 -> 0,1,2,4 */
        if (size == 3) {
            size = 4;
        }
        uint32_t data = 0;
        for (uint8_t b = 0; b < size && i < len; b++) {
            data |= (uint32_t)d[i++] << (8 * b);
        }
        switch (item & 0xFC) {            /* bTag | bType, size bits masked off */
        case 0x04:                        /* Usage Page (global) */
            usage_page = (uint16_t)data;
            break;
        case 0x08:                        /* Usage (local) */
            usage = (uint16_t)data;
            break;
        case 0xA0:                        /* Collection */
            if (usage_page == 0x01 && usage == 0x06) {
                return true;              /* Generic Desktop / Keyboard */
            }
            break;
        default:
            break;
        }
    }
    return false;
}

/* Drives the status LED based on keyboard-connected state. */
static void led_task(void *arg)
{
    (void)arg;
    bool blink_on = false;
    while (1) {
        if (s_kb_ifaces > 0) {
            led_strip_set_pixel(s_led, 0, 0, 20, 0); /* solid green: connected */
            led_strip_refresh(s_led);
            vTaskDelay(pdMS_TO_TICKS(150));
        } else {
            blink_on = !blink_on;                    /* slow blue blink: waiting */
            if (blink_on) {
                led_strip_set_pixel(s_led, 0, 0, 0, 12);
            } else {
                led_strip_clear(s_led);
            }
            led_strip_refresh(s_led);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

/* SPI master: send a KEY_REPORT when a key changes, else a HEARTBEAT every
 * 25 ms so TGT's watchdog stays satisfied (plan.md SPI Protocol). Each
 * full-duplex exchange also harvests TGT's frame (LED_REPORT handled later). */
static void spi_master_task(void *arg)
{
    (void)arg;
    WORD_ALIGNED_ATTR static uint8_t tx[BRIDGE_FRAME_SIZE];
    WORD_ALIGNED_ATTR static uint8_t rx[BRIDGE_FRAME_SIZE];
    uint8_t seq = 0;
    uint8_t rpt[BRIDGE_PAYLOAD_SIZE];

    while (1) {
        bridge_frame_t f = { .seq = seq++ };
        if (xQueueReceive(s_report_queue, rpt, pdMS_TO_TICKS(HEARTBEAT_MS)) == pdTRUE) {
            f.type = BRIDGE_TYPE_KEY_REPORT;
            memcpy(f.payload, rpt, BRIDGE_PAYLOAD_SIZE);
        } else {
            f.type = BRIDGE_TYPE_HEARTBEAT;
        }
        bridge_encode(&f, tx);

        spi_transaction_t t = {
            .length = BRIDGE_FRAME_SIZE * 8,
            .tx_buffer = tx,
            .rx_buffer = rx,
        };
        /* Soft CS, open-drain: assert (sink low), transact, release (hi-Z).
         * CS is the only link line that would otherwise idle HIGH, and at
         * 2.2k series it back-feeds ~460 uA into an unpowered TGT, parking
         * its 3V3 rail at 1.68 V and defeating power-on reset (measured).
         * Open-drain can only sink, so it cannot inject; TGT supplies the
         * idle pull-up from its own rail. */
        gpio_set_level(PIN_CS, 0);
        esp_rom_delay_us(2);                 /* CS setup before first SCK */
        esp_err_t xfer = spi_device_transmit(s_spi, &t);
        esp_rom_delay_us(2);                 /* CS hold after last SCK */
        gpio_set_level(PIN_CS, 1);
        if (xfer == ESP_OK) {
            bridge_frame_t in;
            if (bridge_decode(rx, &in) && in.type == BRIDGE_TYPE_LED_REPORT) {
                static uint8_t last_leds = 0xFF;
                if (in.payload[0] != last_leds) {
                    last_leds = in.payload[0];
                    ESP_LOGI(TAG, "LED state <- 0x%02X", in.payload[0]);
                    kb_set_leds(in.payload[0]);
                }
            }
        }
    }
}

/* USB Host Library event pump. */
static void usb_lib_task(void *arg)
{
    (void)arg;
    while (1) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
}

/* Per-interface events: input reports + disconnect. Runs in HID driver task. */
static void hid_iface_cb(hid_host_device_handle_t handle,
                         const hid_host_interface_event_t event, void *arg)
{
    (void)arg;
    hid_host_dev_params_t params;
    hid_host_device_get_params(handle, &params);

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
        uint8_t data[KB_REPORT_MAX];
        size_t len = 0;
        if (hid_host_device_get_raw_input_report_data(handle, data, sizeof(data), &len) == ESP_OK) {
            if (!kb_report_changed(handle, data, len)) {
                break; /* identical to previous report -> ignore (dedup floods) */
            }
            char hex[3 * sizeof(data) + 1];
            int p = 0;
            for (size_t i = 0; i < len && i < sizeof(data); i++) {
                p += sprintf(hex + p, "%02X ", data[i]);
            }
            /* Boot keyboard report layout: [modifiers][reserved][keycode x6] */
            ESP_LOGI(TAG, "key report iface %d (%d B): %s", params.iface_num, (int)len, hex);

            /* Forward the (up to) 8-byte boot report to TGT over SPI. */
            uint8_t rpt[BRIDGE_PAYLOAD_SIZE] = {0};
            size_t n = (len < BRIDGE_PAYLOAD_SIZE) ? len : BRIDGE_PAYLOAD_SIZE;
            memcpy(rpt, data, n);
            if (s_report_queue) {
                xQueueSend(s_report_queue, rpt, 0);
            }
        }
        break;
    }
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        /* This fires both on real unplug and when we close a non-keyboard
         * interface. Only act (and close) if it was a tracked keyboard. */
        if (kb_track_remove(handle)) {
            ESP_LOGI(TAG, "keyboard disconnected (iface %d)", params.iface_num);
            hid_host_device_close(handle);
            /* Immediately forward an all-keys-up report so nothing sticks on the
             * target, rather than waiting for TGT's watchdog (FR4.2). */
            uint8_t zero[BRIDGE_PAYLOAD_SIZE] = {0};
            if (s_report_queue) {
                xQueueSend(s_report_queue, zero, 0);
            }
        }
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGW(TAG, "transfer error (iface %d)", params.iface_num);
        break;
    default:
        break;
    }
}

/* Driver-level events (device connected). Marshal to the app task. */
static void hid_driver_cb(hid_host_device_handle_t handle,
                          const hid_host_driver_event_t event, void *arg)
{
    (void)arg;
    const app_event_t e = { .handle = handle, .event = event };
    xQueueSend(s_event_queue, &e, 0);
}

static void handle_connected(hid_host_device_handle_t handle)
{
    hid_host_dev_params_t params;
    ESP_ERROR_CHECK(hid_host_device_get_params(handle, &params));

    const hid_host_device_config_t cfg = {
        .callback = hid_iface_cb,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(hid_host_device_open(handle, &cfg));

    /* Decide if this interface is a keyboard: prefer the boot protocol byte,
     * fall back to parsing the report descriptor for non-boot keyboards. */
    bool is_keyboard = (params.proto == HID_PROTOCOL_KEYBOARD);
    const char *how = "boot-proto";
    if (!is_keyboard) {
        size_t desc_len = 0;
        const uint8_t *desc = hid_host_get_report_descriptor(handle, &desc_len);
        if (descriptor_is_keyboard(desc, desc_len)) {
            is_keyboard = true;
            how = "report-desc";
        }
    }

    if (!is_keyboard) {
        ESP_LOGI(TAG, "ignoring non-keyboard iface %d (sub_class=%d proto=%d)",
                 params.iface_num, params.sub_class, params.proto);
        hid_host_device_close(handle);
        return;
    }

    /* If the device offers boot protocol, use it for the clean 8-byte report
     * (FR1.3). Otherwise consume its report-protocol reports as-is. */
    if (params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
        ESP_ERROR_CHECK(hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_BOOT));
    }
    /* Set Idle = 0 => report only on change, for boot AND report-protocol
     * keyboards (stops held-key report floods). Some devices STALL this
     * request; that's non-fatal. */
    esp_err_t idle_err = hid_class_request_set_idle(handle, 0, 0);
    if (idle_err != ESP_OK) {
        ESP_LOGW(TAG, "set_idle not honored (iface %d): %s",
                 params.iface_num, esp_err_to_name(idle_err));
    }

    ESP_ERROR_CHECK(hid_host_device_start(handle));
    kb_track_add(handle);

    ESP_LOGI(TAG, "keyboard iface started: addr=%d iface=%d sub_class=%d proto=%d (via %s)",
             params.addr, params.iface_num, params.sub_class, params.proto, how);
}

void app_main(void)
{
    led_strip_config_t strip_cfg = {
        .strip_gpio_num = WS2812_GPIO,
        .max_leds = 1,
        /* This board's onboard WS2812 uses RGB order (not the usual GRB),
         * so set the format explicitly or red/green come out swapped. */
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
    };
    led_strip_rmt_config_t rmt_cfg = { .resolution_hz = 10 * 1000 * 1000 };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led));
    led_strip_clear(s_led);
    xTaskCreate(led_task, "led", 2048, NULL, 2, NULL);

    /* SPI master link to TGT. */
    s_report_queue = xQueueCreate(16, BRIDGE_PAYLOAD_SIZE);
    const spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    /* CS driven by hand, open-drain, released (hi-Z) at idle. */
    const gpio_config_t cs_cfg = {
        .pin_bit_mask = 1ULL << PIN_CS,
        .mode = GPIO_MODE_OUTPUT_OD,
    };
    ESP_ERROR_CHECK(gpio_config(&cs_cfg));
    gpio_set_level(PIN_CS, 1);

    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST_ID, &buscfg, SPI_DMA_CH_AUTO));
    const spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = -1,          /* soft CS: see spi_master_task */
        .queue_size = 3,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI_HOST_ID, &devcfg, &s_spi));
    xTaskCreate(spi_master_task, "spi", 4096, NULL, 4, NULL);

    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4096, NULL, 2, NULL, 0);

    s_event_queue = xQueueCreate(10, sizeof(app_event_t));

    const hid_host_driver_config_t driver_cfg = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_driver_cb,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(hid_host_install(&driver_cfg));

    ESP_LOGI(TAG, "KBD USB host ready — plug a keyboard into KBD's USB-C port");

    app_event_t e;
    while (1) {
        if (xQueueReceive(s_event_queue, &e, portMAX_DELAY)) {
            if (e.event == HID_HOST_DRIVER_EVENT_CONNECTED) {
                handle_connected(e.handle);
            }
        }
    }
}
