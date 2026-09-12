#include "driver/ledc.h"
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <stdio.h>
#include "esp_err.h"

#define LED1_PIN 38
#define LED2_PIN 39


void app_main() {

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,     
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_10_BIT,        
        .freq_hz          = 5000,                      
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Channel for LED 1
    ledc_channel_config_t ledc_channel1 = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LED1_PIN,                
        .duty           = 0,                         
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel1);

    // Channel for LED 2
    ledc_channel_config_t ledc_channel2 = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LED2_PIN,                 
        .duty           = 0,                         
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel2);

    while (1) {

        // Fade LED1 up, LED2 down
        for (int duty = 0; duty <= 1023; duty += 20) {

            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 1023 - duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Fade LED1 down, LED2 up
        for (int duty = 1023; duty >= 0; duty -= 20) {

            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 1023 - duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

}
