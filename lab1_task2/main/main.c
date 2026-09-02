// 74% of storage used … If you run out of space, you can't save to Drive, back
// up Google Photos, or use Gmail.
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include "esp_timer.h" 

#define BUTTON_PIN 38
#define BUZZER_PIN 39

static volatile int64_t last_isr_us = 0;

static void IRAM_ATTR button_isr(void *arg) {
  int64_t now = esp_timer_get_time();
  if(now - last_isr_us < 50000)
    return;
  last_isr_us = now;
  int level = gpio_get_level(BUTTON_PIN);
  if(level == 1) 
    gpio_set_level(BUZZER_PIN, 1);
  else
    gpio_set_level(BUZZER_PIN, 0);


}

void app_main() {
  // Write your code her
  gpio_config_t button_conf = {
    .pin_bit_mask = (1ULL << BUTTON_PIN),
    .mode = GPIO_MODE_INPUT,
    .intr_type = GPIO_INTR_ANYEDGE
  };
  gpio_config(&button_conf);
  
  gpio_config_t buzzer_config = {
    .pin_bit_mask = (1ULL << BUZZER_PIN),
    .mode = GPIO_MODE_OUTPUT
  };
  gpio_config(&buzzer_config);

  gpio_install_isr_service(0);

  gpio_isr_handler_add(BUTTON_PIN, button_isr, NULL);


}
