#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#define LED_PIN 38

void app_main(void) {
  // Write your code her
    gpio_config_t io_conf = {
         .pin_bit_mask = (1ULL << LED_PIN),
         .mode = GPIO_MODE_OUTPUT,
    };
     gpio_config(&io_conf);

     while (1) {
        //S
         gpio_set_level(LED_PIN, 1);
         vTaskDelay(pdMS_TO_TICKS(500));
         gpio_set_level(LED_PIN, 0);
         vTaskDelay(pdMS_TO_TICKS(500));
         gpio_set_level(LED_PIN, 1);
         vTaskDelay(pdMS_TO_TICKS(500));
         gpio_set_level(LED_PIN, 0);
         vTaskDelay(pdMS_TO_TICKS(500));
         gpio_set_level(LED_PIN, 1);
         vTaskDelay(pdMS_TO_TICKS(500));
         gpio_set_level(LED_PIN, 0);
         vTaskDelay(pdMS_TO_TICKS(500));

        //O
         gpio_set_level(LED_PIN, 1);
         vTaskDelay(pdMS_TO_TICKS(1000));
         gpio_set_level(LED_PIN, 0);
         vTaskDelay(pdMS_TO_TICKS(1000));
         gpio_set_level(LED_PIN, 1);
         vTaskDelay(pdMS_TO_TICKS(1000));
         gpio_set_level(LED_PIN, 0);
         vTaskDelay(pdMS_TO_TICKS(1000));
         gpio_set_level(LED_PIN, 1);
         vTaskDelay(pdMS_TO_TICKS(1000));
         gpio_set_level(LED_PIN, 0);
         vTaskDelay(pdMS_TO_TICKS(1000));

        //S
        gpio_set_level(LED_PIN, 1);
         vTaskDelay(pdMS_TO_TICKS(500));
         gpio_set_level(LED_PIN, 0);
         vTaskDelay(pdMS_TO_TICKS(500));
         gpio_set_level(LED_PIN, 1);
         vTaskDelay(pdMS_TO_TICKS(500));
         gpio_set_level(LED_PIN, 0);
         vTaskDelay(pdMS_TO_TICKS(500));
         gpio_set_level(LED_PIN, 1);
         vTaskDelay(pdMS_TO_TICKS(500));
         gpio_set_level(LED_PIN, 0);
         vTaskDelay(pdMS_TO_TICKS(500));
         


     }


}
