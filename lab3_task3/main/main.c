// ------ ESP-IDF and LVGL includes ------
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "pin_config.h"
#include <stdio.h>

// MPU6050
#include "mpu6050.h"
#include "unity.h"

// Board-specific pin and display configuration
#include "esp32s3_box_lcd_config.h"


static const char *TAG = "lab3_task3";

#define I2C_MASTER_NUM I2C_NUM_0  /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 400000 /*!< I2C master clock frequency */


static mpu6050_handle_t mpu6050_dev = NULL;
static lv_disp_t *disp;

static void i2c_bus_init(void) {
  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = (gpio_num_t)MPU6050_SDA_PIN;
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_io_num = (gpio_num_t)MPU6050_SCL_PIN;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
  conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

  esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
  TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, ret, "I2C config returned error");

  ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
  TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, ret, "I2C install returned error");
}

// MPU6050 sensor setup function
static void mpu6050_setup(void) {
  i2c_bus_init();
  mpu6050_dev = mpu6050_create(0, 0x68u);
  mpu6050_config(mpu6050_dev, ACCE_FS_4G, GYRO_FS_500DPS);
  mpu6050_wake_up(mpu6050_dev);
  ESP_LOGI(TAG, "MPU6050 sensor initialized");
}

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


static volatile bool gyro_read = true;
static volatile int64_t last_isr_us = 0;
static lv_obj_t *label1;
static lv_obj_t *label2;
static lv_obj_t *label3;
#define BUTTON_PIN 38

static void IRAM_ATTR button_isr_handler(void *arg)
{
    int64_t now = esp_timer_get_time();
    if(now - last_isr_us < 50000)
        return;
    last_isr_us = now;

    int level = gpio_get_level(BUTTON_PIN);

    if(level == 0) {
        gyro_read = true;
    }

}


void app_main(void) {
  // Initialize GUI and get display handle
  disp = gui_setup();

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

    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_scr_act();
        label1 = lv_label_create(scr);
        lv_label_set_text(label1, "X_acc -- , X_gyro --");
        lv_obj_align(label1, LV_ALIGN_CENTER, 0, -40);

        label2 = lv_label_create(scr);
        lv_label_set_text(label2, "Y_acc -- , Y_gyro --");
        lv_obj_align(label2, LV_ALIGN_CENTER, 0, 0);

        label3 = lv_label_create(scr);
        lv_label_set_text(label3, "Z_acc -- , Z_gyro --");
        lv_obj_align(label3, LV_ALIGN_CENTER, 0, 40);

        lvgl_port_unlock();
    }

  // Initialize MPU6050 sensor
  mpu6050_setup();

    while(1) {
      if(gyro_read) {
        gyro_read = false;

        mpu6050_acce_value_t acce;
        mpu6050_gyro_value_t gyro;

        mpu6050_get_acce(mpu6050_dev, &acce);
        mpu6050_get_gyro(mpu6050_dev, &gyro);

        char text1[50];
        char text2[50];
        char text3[50];

        sprintf(text1, "X_acc %.1f, X_gyro %.1f", acce.acce_x, gyro.gyro_x);
        sprintf(text2, "Y_acc %.1f, Y_gyro %.1f", acce.acce_y, gyro.gyro_y);
        sprintf(text3, "Z_acc %.1f, Z_gyro %.1f", acce.acce_z, gyro.gyro_z);

        if (lvgl_port_lock(0)) {
            lv_label_set_text(label1, text1);
            lv_label_set_text(label2, text2);
            lv_label_set_text(label3, text3);
            lvgl_port_unlock();

        
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }

    


}
