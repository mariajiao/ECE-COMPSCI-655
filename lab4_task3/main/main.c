#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include <stdbool.h>
#include <stdio.h>

// Board-specific pin and display configuration
#include "esp32s3_box_lcd_config.h"

static const char *TAG = "lab4_task3";
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
static char http_response_buffer[4096];

static int s_retry_num = 0;

static lv_disp_t *disp;
static EventGroupHandle_t s_wifi_event_group;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
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

esp_err_t wifi_init_sta() {
  esp_err_t ret_value = ESP_OK;
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = "DukeVisitor",
              // .password = CONFIG_ESP_WIFI_PASSWORD
              
          },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
   * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
   * bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, // xClearOnExit
                                         pdFALSE, // xWaitForAllBits
                                         portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we
   * can test which event actually happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_SSID,
             CONFIG_ESP_WIFI_PASSWORD);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:%s",
             CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    ret_value = ESP_FAIL;
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
    ret_value = ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "wifi_init_sta finished.");
  ESP_LOGI(TAG, "connect to ap SSID:%s", CONFIG_ESP_WIFI_SSID);
  vEventGroupDelete(s_wifi_event_group);
  return ret_value;
}


static lv_obj_t *label1;

static bool get_location_json(const char *url) {
  esp_http_client_config_t config = {
      .url = url,
      .method = HTTP_METHOD_GET,
      .timeout_ms = 10000,
  };

  http_response_buffer[0] = '\0';
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL) {
    return false;
  }

  esp_err_t err = esp_http_client_open(client, 0);
  int status = -1;
  int read_len = -1;
  if (err == ESP_OK) {
    // Read the headers before reading the response body. This is more
    // reliable for both normal and chunked HTTP responses.
    esp_http_client_fetch_headers(client);
    status = esp_http_client_get_status_code(client);
    if (status >= 200 && status < 300) {
      read_len = esp_http_client_read_response(
          client, http_response_buffer, sizeof(http_response_buffer) - 1);
    }
  }
  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  if (read_len <= 0) {
    ESP_LOGE(TAG, "GET %s failed (err=%s, status=%d)", url,
             esp_err_to_name(err), status);
    return false;
  }
  http_response_buffer[read_len] = '\0';
  ESP_LOGI(TAG, "GET %s returned %d bytes: %s", url, read_len,
           http_response_buffer);
  return true;
}

static cJSON *first_json_item(cJSON *json, const char *name1,
                              const char *name2) {
  cJSON *item = cJSON_GetObjectItemCaseSensitive(json, name1);
  return item != NULL ? item : cJSON_GetObjectItemCaseSensitive(json, name2);
}

void app_main() {
  // Initialize GUI and get display handle
  disp = gui_setup();
  if (lvgl_port_lock(0)) {
      lv_obj_t *scr = lv_scr_act();
      label1 = lv_label_create(scr);
      lv_obj_align(label1, LV_ALIGN_CENTER, 0, -80);

      lvgl_port_unlock();
  }

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  
  

  if (wifi_init_sta() != ESP_OK) {
    ESP_LOGE(TAG, "Connection failed");
    while (1) {
      vTaskDelay(1);
    }
  }

    const char *urls[] = {"http://ip-api.com/json", "http://ipapi.co/json",
                          "http://ipinfo.io/json"};
    cJSON *json = NULL;
    for (size_t i = 0; i < sizeof(urls) / sizeof(urls[0]); ++i) {
      if (get_location_json(urls[i])) {
        json = cJSON_Parse(http_response_buffer);
        if (json != NULL) break;
      }
      ESP_LOGW(TAG, "Trying the next location service");
    }

    if (json != NULL) {
      cJSON *zip_code = first_json_item(json, "zip", "postal");
      cJSON *lat_item = cJSON_GetObjectItemCaseSensitive(json, "lat");
      cJSON *lon_item = cJSON_GetObjectItemCaseSensitive(json, "lon");
      cJSON *loc = cJSON_GetObjectItemCaseSensitive(json, "loc");
      double lat = cJSON_IsNumber(lat_item) ? lat_item->valuedouble : 0.0;
      double lon = cJSON_IsNumber(lon_item) ? lon_item->valuedouble : 0.0;

      if (cJSON_IsString(loc) && loc->valuestring &&
          (lat_item == NULL || lon_item == NULL)) {
        sscanf(loc->valuestring, "%lf,%lf", &lat, &lon);
      }

      char text[128];
      snprintf(text, sizeof(text), "Zip Code: %s\nLat: %.4f\nLon: %.4f",
               cJSON_IsString(zip_code) ? zip_code->valuestring : "N/A", lat,
               lon);
      ESP_LOGI(TAG, "%s", text);
      if (lvgl_port_lock(0)) {
        lv_label_set_text(label1, text);
        lvgl_port_unlock();
      }
      cJSON_Delete(json);
    } else {
      ESP_LOGE(TAG, "All location services failed or returned invalid JSON");
      if (lvgl_port_lock(0)) {
        lv_label_set_text(label1, "Location unavailable");
        lvgl_port_unlock();
      }
    }

    



}
