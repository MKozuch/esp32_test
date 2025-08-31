#pragma once

#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "memory"
#include <memory>
#include <string>

class WifiClient {
   static constexpr auto TAG = "WifiClient";
   const std::string ssid = "";
   const std::string password = "";
   esp_netif_t *netif_handle = nullptr;

   WifiClient() = default;
   WifiClient(const WifiClient &) = delete;
   WifiClient &operator=(const WifiClient &) = delete;
   WifiClient(WifiClient &&) = delete;
   WifiClient &operator=(WifiClient &&) = delete;

   static std::weak_ptr<WifiClient> m_instance;
   std::mutex mtx; // TODO

   friend class std::shared_ptr<WifiClient>;
   friend std::shared_ptr<WifiClient> std::make_shared<WifiClient>();

 public:
   static std::shared_ptr<WifiClient> get_instance()
   {
      if(not m_instance.expired())
         return m_instance.lock();

      auto shared_instance = std::shared_ptr<WifiClient>(new WifiClient());
      m_instance = shared_instance; 
      return shared_instance;
   }

   void init()
   {
      ESP_ERROR_CHECK(esp_netif_init());
      ESP_ERROR_CHECK(esp_event_loop_create_default());

      netif_handle = esp_netif_create_default_wifi_sta();

      wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
      ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

      ESP_ERROR_CHECK(esp_event_handler_instance_register(
          WIFI_EVENT,
          ESP_EVENT_ANY_ID,
          &WifiClient::wifi_event_handler_static,
          this,
          nullptr));

      ESP_ERROR_CHECK(esp_event_handler_instance_register(
          IP_EVENT,
          ESP_EVENT_ANY_ID,
          &WifiClient::ip_event_handler_static,
          this,
          nullptr));

      ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode_t::WIFI_MODE_STA));
      ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
      ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
      ESP_ERROR_CHECK(esp_wifi_start());
   }

   void connect()
   {
      wifi_config_t wifi_config = {
          .sta = {}};
      strncpy((char *)wifi_config.sta.ssid, ssid.data(), sizeof(wifi_config.sta.ssid));
      strncpy((char *)wifi_config.sta.password, password.data(), sizeof(wifi_config.sta.password));

      ESP_ERROR_CHECK(esp_wifi_set_config(wifi_interface_t::WIFI_IF_STA, &wifi_config));
      ESP_ERROR_CHECK(esp_wifi_connect());
   }

   void log_status()
   {
      wifi_ap_record_t ap_info;
      const auto ret = esp_wifi_sta_get_ap_info(&ap_info);

      if (ret == ESP_ERR_WIFI_CONN) {
         ESP_LOGE(TAG, "Wi-Fi station interface not initialized");
      } else if (ret == ESP_ERR_WIFI_NOT_CONNECT) {
         ESP_LOGE(TAG, "Wi-Fi station is not connected");
      } else {
         ESP_LOGI(TAG, "--- Access Point Information ---");
         ESP_LOG_BUFFER_HEX("MAC Address", ap_info.bssid, sizeof(ap_info.bssid));
         ESP_LOG_BUFFER_CHAR("SSID", ap_info.ssid, sizeof(ap_info.ssid));
         ESP_LOGI(TAG, "Primary Channel: %d", ap_info.primary);
         ESP_LOGI(TAG, "RSSI: %d", ap_info.rssi);
      }
   }

   void wifi_event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data)
   {
      if (event_id == WIFI_EVENT_STA_START) {
         ESP_LOGI(TAG, "WiFi started, connecting to AP...");
         connect();
      } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
         ESP_LOGI(TAG, "Disconnected from WiFi, attempting to reconnect...");
         connect();
      } else if (event_id == IP_EVENT_STA_GOT_IP) {
         ip_event_got_ip_t *event = static_cast<ip_event_got_ip_t *>(event_data);
         ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
      } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
         ESP_LOGI(TAG, "Connected to WiFi");
      } else {
         ESP_LOGI(TAG, "Unhandled WiFi event: %d", event_id);
      }
   }

   static void wifi_event_handler_static(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
   {
      WifiClient *self = static_cast<WifiClient *>(arg);
      self->wifi_event_handler(event_base, event_id, event_data);
   }

   void ip_event_handler(esp_event_base_t event_base, int32_t event_id, void *event_data)
   {
      if (event_id == IP_EVENT_STA_GOT_IP) {
         ip_event_got_ip_t *event = static_cast<ip_event_got_ip_t *>(event_data);
         ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
      } else if (event_id == IP_EVENT_STA_LOST_IP) {
         ESP_LOGI(TAG, "Lost IP Address");
      } else if (event_id == IP_EVENT_GOT_IP6) {
         ip_event_got_ip6_t *event = static_cast<ip_event_got_ip6_t *>(event_data);
         ESP_LOGI(TAG, "Got IPv6 Address: " IPV6STR, IPV62STR(event->ip6_info.ip));
      } else {
         ESP_LOGI(TAG, "Unhandled IP event: %d", event_id);
      }
   }

   static void ip_event_handler_static(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
   {
      WifiClient *self = static_cast<WifiClient *>(arg);
      self->ip_event_handler(event_base, event_id, event_data);
   }
};

std::weak_ptr<WifiClient> WifiClient::m_instance{};