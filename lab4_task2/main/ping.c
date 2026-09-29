#include "esp_check.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"

static const char *TAG = "PING";
extern void update_lcd_ping(const char *ip_str, uint32_t delay_ms);

static void cmd_ping_on_ping_success(esp_ping_handle_t hdl, void *args) {
  uint8_t ttl;
  uint16_t seqno;
  uint32_t elapsed_time, recv_len;
  ip_addr_t target_addr;
  

  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
                       sizeof(target_addr));
  esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time,
                       sizeof(elapsed_time));
  printf("%" PRIu32 " bytes from %s icmp_seq=%" PRIu16 " ttl=%" PRIu16
         " time=%" PRIu32 " ms\n",
         recv_len, ipaddr_ntoa((ip_addr_t *)&target_addr), seqno, ttl,
         elapsed_time);
  update_lcd_ping(ipaddr_ntoa(&target_addr), elapsed_time);
}

static void cmd_ping_on_ping_timeout(esp_ping_handle_t hdl, void *args) {
  uint16_t seqno;
  ip_addr_t target_addr;
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
                       sizeof(target_addr));
  printf("From %s icmp_seq=%d timeout\n",
         ipaddr_ntoa((ip_addr_t *)&target_addr), seqno);
}

static void cmd_ping_on_ping_end(esp_ping_handle_t hdl, void *args) {
  ip_addr_t target_addr;
  uint32_t transmitted;
  uint32_t received;
  uint32_t total_time_ms;
  uint32_t loss;

  esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted,
                       sizeof(transmitted));
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
                       sizeof(target_addr));
  esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms,
                       sizeof(total_time_ms));

  if (transmitted > 0) {
    loss = (uint32_t)((1 - ((float)received) / transmitted) * 100);
  } else {
    loss = 0;
  }
#ifdef CONFIG_LWIP_IPV4
  if (IP_IS_V4(&target_addr)) {
    printf("\n--- %s ping statistics ---\n",
           inet_ntoa(*ip_2_ip4(&target_addr)));
  }
#endif
#ifdef CONFIG_LWIP_IPV6
  if (IP_IS_V6(&target_addr)) {
    printf("\n--- %s ping statistics ---\n",
           inet6_ntoa(*ip_2_ip6(&target_addr)));
  }
#endif
  printf("%" PRIu32 " packets transmitted, %" PRIu32 " received, %" PRIu32
         "%% packet loss, time %" PRIu32 "ms\n",
         transmitted, received, loss, total_time_ms);
  // delete the ping sessions, so that we clean up all resources and can create
  // a new ping session we don't have to call delete function in the callback,
  // instead we can call delete function from other tasks
  esp_ping_delete_session(hdl);
}

esp_err_t initialize_ping(uint32_t interval_ms, uint32_t task_prio,
                          char *target_host) {
  esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();

  if (strlen(target_host) > 0) {
    /* convert URL to IP address */
    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    struct addrinfo hint;
    memset(&hint, 0, sizeof(hint));
    struct addrinfo *res = NULL;
    int err = getaddrinfo(target_host, NULL, &hint, &res);
    ESP_LOGI(TAG, "Get error info %d", err);

    if (err != 0 || res == NULL) {
      ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
      return ESP_FAIL;
    } else {
      ESP_LOGI(TAG, "DNS lookup success");
    }

    if (res->ai_family == AF_INET) {
      struct in_addr addr4 = ((struct sockaddr_in *)(res->ai_addr))->sin_addr;
      inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    } else {
      struct in6_addr addr6 =
          ((struct sockaddr_in6 *)(res->ai_addr))->sin6_addr;
      inet6_addr_to_ip6addr(ip_2_ip6(&target_addr), &addr6);
    }
    freeaddrinfo(res);
    ESP_LOGI(TAG, "target_addr.type=%d", target_addr.type);
    ESP_LOGI(TAG, "target_addr.u_addr.ip4=%s",
             ip4addr_ntoa(&(target_addr.u_addr.ip4)));
    ping_config.target_addr = target_addr; // target IP address
  }

  ping_config.count = ESP_PING_COUNT_INFINITE; // ping in infinite mode,
                                               // esp_ping_stop can stop it
  ping_config.interval_ms = interval_ms;
  ping_config.task_prio = task_prio;

  /* set callback functions */
  esp_ping_callbacks_t cbs = {.on_ping_success = cmd_ping_on_ping_success,
                              .on_ping_timeout = cmd_ping_on_ping_timeout,
                              .on_ping_end = cmd_ping_on_ping_end,
                              .cb_args = NULL};
  esp_ping_handle_t ping;
  esp_ping_new_session(&ping_config, &cbs, &ping);
  esp_ping_start(ping);
  ESP_LOGI(TAG, "ping start");
  return ESP_OK;
}

int do_ping_cmd(char *target_host, uint32_t interval_ms) {
  esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();

  // Set interval from parameter
  config.interval_ms = interval_ms;
  config.count = ESP_PING_COUNT_INFINITE; // ping in infinite mode

  // parse IP address from target_host parameter
  ip_addr_t target_addr;
  memset(&target_addr, 0, sizeof(target_addr));
  struct addrinfo hint;
  memset(&hint, 0, sizeof(hint));
  struct addrinfo *res = NULL;

  int err = getaddrinfo(target_host, NULL, &hint, &res);
  if (err != 0 || res == NULL) {
    ESP_LOGE(TAG, "DNS lookup failed for %s, err=%d res=%p", target_host, err,
             res);
    return 1;
  }

#ifdef CONFIG_LWIP_IPV4
  if (res->ai_family == AF_INET) {
    struct in_addr addr4 = ((struct sockaddr_in *)(res->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
  }
#endif
#ifdef CONFIG_LWIP_IPV6
  if (res->ai_family == AF_INET6) {
    struct in6_addr addr6 = ((struct sockaddr_in6 *)(res->ai_addr))->sin6_addr;
    inet6_addr_to_ip6addr(ip_2_ip6(&target_addr), &addr6);
  }
#endif
  freeaddrinfo(res);

  // Set target address from parameter
  config.target_addr = target_addr;

  /* set callback functions */
  esp_ping_callbacks_t cbs = {.cb_args = NULL,
                              .on_ping_success = cmd_ping_on_ping_success,
                              .on_ping_timeout = cmd_ping_on_ping_timeout,
                              .on_ping_end = cmd_ping_on_ping_end};
  esp_ping_handle_t ping;
  ESP_RETURN_ON_FALSE(esp_ping_new_session(&config, &cbs, &ping) == ESP_OK, -1,
                      TAG, "esp_ping_new_session failed");
  ESP_RETURN_ON_FALSE(esp_ping_start(ping) == ESP_OK, -1, TAG,
                      "esp_ping_start() failed");
  return 0;
}
