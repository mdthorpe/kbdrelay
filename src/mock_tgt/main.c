/* mock_tgt firmware — SPI slave stand-in for testing KBD standalone (task T4).
 * Logs decoded frames over UART and queues scripted LED_REPORTs. Scaffold stub. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bridge_proto.h"

static const char *TAG = "mock_tgt";

void app_main(void)
{
    ESP_LOGI(TAG, "mock_tgt scaffold up (SPI slave stand-in)");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
