
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "regex.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "pin_config.h"
#include <string.h>
#include "lvgl.h"
#include "esp32s3_box_lcd_config.h"
#include "driver/gpio.h"

#define DEFAULT_SCAN_LIST_SIZE 5

#ifdef CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP
#define USE_CHANNEL_BITMAP 1
#define CHANNEL_LIST_SIZE 3
static uint8_t channel_list[CHANNEL_LIST_SIZE] = {1, 6, 11};
#endif /*CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP*/

static const char *TAG = "lab4_task1";
static lv_disp_t *disp;
static lv_obj_t *label1;
static lv_obj_t *label2;

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


static void print_auth_mode(int authmode) {
  switch (authmode) {
  case WIFI_AUTH_OPEN:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
    break;
  case WIFI_AUTH_OWE:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OWE");
    break;
  case WIFI_AUTH_WEP:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
    break;
  case WIFI_AUTH_WPA_PSK:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
    break;
  case WIFI_AUTH_WPA2_PSK:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
    break;
  case WIFI_AUTH_WPA_WPA2_PSK:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
    break;
  case WIFI_AUTH_ENTERPRISE:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_ENTERPRISE");
    break;
  case WIFI_AUTH_WPA3_PSK:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
    break;
  case WIFI_AUTH_WPA2_WPA3_PSK:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
    break;
  case WIFI_AUTH_WPA3_ENTERPRISE:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENTERPRISE");
    break;
  case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_ENTERPRISE");
    break;
  case WIFI_AUTH_WPA3_ENT_192:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENT_192");
    break;
  default:
    ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
    break;
  }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher) {
  switch (pairwise_cipher) {
  case WIFI_CIPHER_TYPE_NONE:
    ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
    break;
  case WIFI_CIPHER_TYPE_WEP40:
    ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
    break;
  case WIFI_CIPHER_TYPE_WEP104:
    ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
    break;
  case WIFI_CIPHER_TYPE_TKIP:
    ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
    break;
  case WIFI_CIPHER_TYPE_CCMP:
    ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
    break;
  case WIFI_CIPHER_TYPE_TKIP_CCMP:
    ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
    break;
  case WIFI_CIPHER_TYPE_AES_CMAC128:
    ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_AES_CMAC128");
    break;
  case WIFI_CIPHER_TYPE_SMS4:
    ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_SMS4");
    break;
  case WIFI_CIPHER_TYPE_GCMP:
    ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP");
    break;
  case WIFI_CIPHER_TYPE_GCMP256:
    ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP256");
    break;
  default:
    ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
    break;
  }

  switch (group_cipher) {
  case WIFI_CIPHER_TYPE_NONE:
    ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
    break;
  case WIFI_CIPHER_TYPE_WEP40:
    ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
    break;
  case WIFI_CIPHER_TYPE_WEP104:
    ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
    break;
  case WIFI_CIPHER_TYPE_TKIP:
    ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
    break;
  case WIFI_CIPHER_TYPE_CCMP:
    ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
    break;
  case WIFI_CIPHER_TYPE_TKIP_CCMP:
    ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
    break;
  case WIFI_CIPHER_TYPE_SMS4:
    ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_SMS4");
    break;
  case WIFI_CIPHER_TYPE_GCMP:
    ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP");
    break;
  case WIFI_CIPHER_TYPE_GCMP256:
    ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP256");
    break;
  default:
    ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
    break;
  }
}

#ifdef USE_CHANNEL_BITMAP
static void array_2_channel_bitmap(const uint8_t channel_list[],
                                   const uint8_t channel_list_size,
                                   wifi_scan_config_t *scan_config) {

  for (uint8_t i = 0; i < channel_list_size; i++) {
    uint8_t channel = channel_list[i];
    scan_config->channel_bitmap.ghz_2_channels |= (1 << channel);
  }
}
#endif /*USE_CHANNEL_BITMAP*/

/* Initialize Wi-Fi as sta and set scan method */
static void wifi_scan(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  uint16_t number = DEFAULT_SCAN_LIST_SIZE;
  wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
  uint16_t ap_count = 0;
  memset(ap_info, 0, sizeof(ap_info));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

#ifdef USE_CHANNEL_BITMAP
  wifi_scan_config_t *scan_config =
      (wifi_scan_config_t *)calloc(1, sizeof(wifi_scan_config_t));
  if (!scan_config) {
    ESP_LOGE(TAG, "Memory Allocation for scan config failed!");
    return;
  }
  array_2_channel_bitmap(channel_list, CHANNEL_LIST_SIZE, scan_config);
  esp_wifi_scan_start(scan_config, true);
  free(scan_config);

#else
  esp_wifi_scan_start(NULL, true);
#endif /*USE_CHANNEL_BITMAP*/

  ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
  ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u",
           ap_count, number);
  for (int i = 0; i < number; i++) {
    ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
    ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
    print_auth_mode(ap_info[i].authmode);
    if (ap_info[i].authmode != WIFI_AUTH_WEP) {
      print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
    }
    ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
  }

  char text1[30];
  char text2[256] = "";

  sprintf(text1, "Total Scanned: %d", ap_count);

  for(int i = 0; i < number; i++) {
    sprintf(text2 + strlen(text2), "%d: SSID: %s, RSSI: %d \n", i, (char *)ap_info[i].ssid, ap_info[i].rssi);
  }

  if (lvgl_port_lock(0)) {
      lv_label_set_text(label1, text1);
      lv_label_set_text(label2, text2);
      lvgl_port_unlock();
  }
}



void app_main(void) {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  //display
  disp = gui_setup();
  if (lvgl_port_lock(0)) {
      lv_obj_t *scr = lv_scr_act();
      label1 = lv_label_create(scr);
      lv_obj_align(label1, LV_ALIGN_CENTER, 0, -80);

      label2 = lv_label_create(scr);
      lv_obj_align(label2, LV_ALIGN_CENTER, 0, -20);

      lvgl_port_unlock();
  }
  wifi_scan();

  
  
}
