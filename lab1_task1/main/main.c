#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#define LED_PIN1 38
#define LED_PIN2 39

static volatile bool led_on1 = false;
static volatile bool led_on2 = false;

static void your_interupt_function1(void* arg) {
    // do something here
    led_on1 = !led_on1;
    gpio_set_level(LED_PIN1, led_on1);

}

static void your_interupt_function2(void* arg) {
    // do something here
    led_on2 = !led_on2;
    gpio_set_level(LED_PIN2, led_on2);

}




void app_main() {

  // Write your code her

  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << LED_PIN1 | 1ULL << LED_PIN2),
    .mode = GPIO_MODE_OUTPUT,
  };
  gpio_config(&io_conf);

  // LED1
  esp_timer_create_args_t timer_args1 = {
    .callback = &your_interupt_function1,
    .name = "timer",
  };
  esp_timer_handle_t timer1;
  esp_timer_create(&timer_args1, &timer1);
  esp_timer_start_periodic(timer1, 500*1000);


  // LED2
  esp_timer_create_args_t timer_args2 = {
    .callback = &your_interupt_function2,
    .name = "timer",
  };
  esp_timer_handle_t timer2;
  esp_timer_create(&timer_args2, &timer2);
  esp_timer_start_periodic(timer2, 1000*1000);

}

