// ------ ESP-IDF and LVGL includes ------
#include "dht11.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "pin_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>


static const char *TAG = "lab3_task1";
#define BUTTON_PIN 38


// Board-specific pin and display configuration
#include "esp32s3_box_lcd_config.h"

static lv_disp_t *disp;

// GUI setup function
static lv_disp_t *gui_setup(void) {
  ESP_LOGI(TAG, "Turn off LCD backlight");
  gpio_config_t bk_gpio_config = {.mode = GPIO_MODE_OUTPUT,
                                  .pin_bit_mask = 1ULL
                                                  << EXAMPLE_PIN_NUM_BK_LIGHT};
  ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

  ESP_LOGI(TAG, "Initialize SPI bus");
  spi_bus_config_t bus_config = {
      .sclk_io_num = EXAMPLE_PIN_NUM_SCLK,
      .mosi_io_num = EXAMPLE_PIN_NUM_MOSI,
      .miso_io_num = EXAMPLE_PIN_NUM_MISO,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t),
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

  ESP_LOGI(TAG, "Install panel IO");
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .dc_gpio_num = EXAMPLE_PIN_NUM_LCD_DC,
      .cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,
      .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
      .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
      .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
      .spi_mode = 0,
      .trans_queue_depth = 10,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                           &io_config, &io_handle));

  ESP_LOGI(TAG, "Install ILI9341 panel driver");
  esp_lcd_panel_handle_t panel_handle = NULL;
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
      .flags.reset_active_high = 1,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

  ESP_LOGI(TAG, "Turn on LCD backlight");
  gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);

  ESP_LOGI(TAG, "Initialize LVGL library");
  const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
  lvgl_port_init(&lvgl_cfg);

  const lvgl_port_display_cfg_t disp_cfg = {
      .io_handle = io_handle,
      .panel_handle = panel_handle,
      .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_DRAW_BUF_LINES,
      .double_buffer = true,
      .hres = EXAMPLE_LCD_H_RES,
      .vres = EXAMPLE_LCD_V_RES,
      .monochrome = false,
      .flags = {.swap_bytes = true},
      .rotation = {
          .swap_xy = false,
          .mirror_x = true,
          .mirror_y = true,
      }};
  lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
  return disp;
}

static volatile bool dht_read = true;
static volatile int64_t last_isr_us = 0;
static lv_obj_t *label1;
static lv_obj_t *label2;

static void IRAM_ATTR button_isr_handler(void *arg)
{
    int64_t now = esp_timer_get_time();
    if(now - last_isr_us < 50000)
        return;
    last_isr_us = now;

    int level = gpio_get_level(BUTTON_PIN);

    if(level == 0) {
        dht_read = true;
    }

}

void app_main(void) {
    DHT11_init(DIGITAL_TEMP_PIN);
     //initialize button 
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&button_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);


    //display
    disp = gui_setup();
    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_scr_act();
        label1 = lv_label_create(scr);
        lv_label_set_text(label1, "Temp: -- °F");
        lv_obj_align(label1, LV_ALIGN_CENTER, 0, -20);

        label2 = lv_label_create(scr);
        lv_label_set_text(label2, "RH: -- %");
        lv_obj_align(label2, LV_ALIGN_CENTER, 0, 20);

        lvgl_port_unlock();
    }


    while (1) {

        if (dht_read) {
            dht_read = false;

            struct dht11_reading data = DHT11_read();

            float temp_f = data.temperature * 9.0 / 5.0 + 32.0;

            char temp_text[30];
            char humidity_text[30];

            sprintf(temp_text, "Temp: %.1f °F", temp_f);
            sprintf(humidity_text, "RH: %d %%", data.humidity);

            if (lvgl_port_lock(0)) {
                lv_label_set_text(label1, temp_text);
                lv_label_set_text(label2, humidity_text);
                lvgl_port_unlock();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }




}