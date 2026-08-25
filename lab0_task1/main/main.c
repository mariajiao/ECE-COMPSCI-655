#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#define LED_PIN 38

 void app_main() {
     // Configure the LED pin
     gpio_config_t io_conf = {
         .pin_bit_mask = (1ULL << LED_PIN),
         .mode = GPIO_MODE_OUTPUT,
     };
     gpio_config(&io_conf);

     while (1) {
         gpio_set_level(LED_PIN, 1);
         vTaskDelay(pdMS_TO_TICKS(500));
         gpio_set_level(LED_PIN, 0);
         vTaskDelay(pdMS_TO_TICKS(500));


     }
 }