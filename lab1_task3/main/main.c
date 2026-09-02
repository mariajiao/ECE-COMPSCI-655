#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

esp_timer_handle_t timer1;
esp_timer_handle_t timer2;
esp_timer_handle_t timer3;

#define BUTTON_PIN 38
#define BUZZER_PIN 39
#define LED_PIN 40

static volatile int count = 0;
static volatile bool led_on = false;
static volatile int64_t last_isr_us = 0;
static volatile bool button_pressed = false;

static void interrupt(void* arg) {
    // do something here
    led_on = !led_on;
    gpio_set_level(LED_PIN, led_on);

}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
  // Write your ISR here
  int64_t now = esp_timer_get_time();
  if(now - last_isr_us < 50000)
    return;
  last_isr_us = now;

  int level = gpio_get_level(BUTTON_PIN);

  if(level == 1) {
    gpio_set_level(BUZZER_PIN, 1);
    button_pressed = true;
  }
  else
    gpio_set_level(BUZZER_PIN, 0);
  
  

}

void app_main(void) {
  // Write your code her

  // configure input/outputs
  gpio_config_t button_conf = {
    .pin_bit_mask = (1ULL << BUTTON_PIN),
    .mode = GPIO_MODE_INPUT,
    .intr_type = GPIO_INTR_ANYEDGE,
  };
  gpio_config(&button_conf);
  
  gpio_config_t buzzer_config = {
    .pin_bit_mask = (1ULL << BUZZER_PIN),
    .mode = GPIO_MODE_OUTPUT
  };
  gpio_config(&buzzer_config);

  gpio_config_t led_config = {
    .pin_bit_mask = (1ULL << LED_PIN),
    .mode = GPIO_MODE_OUTPUT
  };
  gpio_config(&led_config);



  gpio_install_isr_service(0);

  gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, NULL);



  // timer 1
  esp_timer_create_args_t timer_args1 = {
    .callback = &interrupt,
    .name = "timer1",
  };
  esp_timer_create(&timer_args1, &timer1);


  //timer 2
  esp_timer_create_args_t timer_args2 = {
    .callback = &interrupt,
    .name = "timer2",
  };
  esp_timer_create(&timer_args2, &timer2);

  //timer 3
  esp_timer_create_args_t timer_args3 = {
    .callback = &interrupt,
    .name = "timer3",
  };
  esp_timer_create(&timer_args3, &timer3);


  while(1) {
    if (button_pressed) {
        button_pressed = false;

        count++;

        if (count == 1) {
          esp_timer_start_periodic(timer1, 500000);
        } else if (count == 2) {
          esp_timer_stop(timer1);
          esp_timer_start_periodic(timer2, 250000);
        } else if (count == 3) {
          esp_timer_stop(timer2);
          esp_timer_start_periodic(timer3, 125000);
        } else if (count == 4) {
          esp_timer_stop(timer3);
          led_on = false;
          gpio_set_level(LED_PIN, 0);
          count = 0;
        }
      }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}