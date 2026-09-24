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

static const char *TAG = "lab3_task4";

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

static lv_obj_t *label1;
static lv_obj_t *label2;
static lv_obj_t *label3;
static volatile bool add_new_reading = true;

static float x_acce[10] = {0};
static float y_acce[10] = {0};
static float z_acce[10] = {0};
static float x_gyro[10] = {0};
static float y_gyro[10] = {0};
static float z_gyro[10] = {0};

static size_t buf_index = 0;
static size_t counter = 0;

static void timer_isr_handler(void *arg)
{
    add_new_reading = true;
}


void app_main(void) {
  // Initialize GUI and get display handle
  disp = gui_setup();

    // Initialize MPU6050 sensor
  mpu6050_setup();

  //create timer for 100 ms
  esp_timer_create_args_t timer_args1 = {
    .callback = &timer_isr_handler,
    .name = "timer",
  };
  esp_timer_handle_t timer1;
  esp_timer_create(&timer_args1, &timer1);
  esp_timer_start_periodic(timer1, 100*1000);

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






  while (1) {

    if(add_new_reading){
      add_new_reading = false;

      mpu6050_acce_value_t acce;
      mpu6050_gyro_value_t gyro;

      mpu6050_get_acce(mpu6050_dev, &acce);
      mpu6050_get_gyro(mpu6050_dev, &gyro);

      x_acce[buf_index] = acce.acce_x;
      y_acce[buf_index] = acce.acce_y;
      z_acce[buf_index] = acce.acce_z;

      x_gyro[buf_index] = gyro.gyro_x;
      y_gyro[buf_index] = gyro.gyro_y;
      z_gyro[buf_index] = gyro.gyro_z;

      buf_index = (buf_index + 1) % 10;
      if (counter < 10) {
        counter++;
      }

      float x_acce_sum = 0.0f, y_acce_sum = 0.0f, z_acce_sum = 0.0f;
      float x_gyro_sum = 0.0f, y_gyro_sum = 0.0f, z_gyro_sum = 0.0f;

      for(int i = 0; i < counter; i++) {
        x_acce_sum += x_acce[i];
        y_acce_sum += y_acce[i];
        z_acce_sum += z_acce[i];

        x_gyro_sum += x_gyro[i];
        y_gyro_sum += y_gyro[i];
        z_gyro_sum += z_gyro[i];

      }
      float x_acc_avg = x_acce_sum / counter;
      float y_acc_avg = y_acce_sum / counter;
      float z_acc_avg = z_acce_sum / counter;

      float x_gyro_avg = x_gyro_sum / counter;
      float y_gyro_avg = y_gyro_sum / counter;
      float z_gyro_avg = z_gyro_sum / counter;

if (lvgl_port_lock(0)) {
        char buf[64];

        snprintf(buf, sizeof(buf), "X_acc %.1f, X_gyro %.1f", x_acc_avg, x_gyro_avg);
        lv_label_set_text(label1, buf);

        snprintf(buf, sizeof(buf), "Y_acc %.1f, Y_gyro %.1f", y_acc_avg, y_gyro_avg);
        lv_label_set_text(label2, buf);

        snprintf(buf, sizeof(buf), "Z_acc %.1f, Z_gyro %.1f", z_acc_avg, z_gyro_avg);
        lv_label_set_text(label3, buf);

        lvgl_port_unlock();
      }


    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
