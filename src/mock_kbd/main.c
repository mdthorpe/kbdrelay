/* mock_kbd firmware — SPI master stand-in for testing TGT standalone (task T3).
 * Scripts KEY_REPORT sequences + heartbeats over the harness. Scaffold stub. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bridge_proto.h"

static const char *TAG = "mock_kbd";

void app_main(void)
{
    ESP_LOGI(TAG, "mock_kbd scaffold up (SPI master stand-in)");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
