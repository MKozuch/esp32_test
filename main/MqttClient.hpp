#pragma once

#include "esp_log.h"
#include "mqtt_client.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <ranges>
#include <string>
#include <set>

class MqttClient {
   static constexpr auto TAG = "MqttClient";

   using MessageHandler_t = void (*)(const std::string &topic, const std::string &data);
   std::multimap<std::string, MessageHandler_t> message_handlers;

 public:
   esp_mqtt_client_handle_t m_client = nullptr;

   static void mqtt_event_handler_static(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
   {
      MqttClient *self = static_cast<MqttClient *>(handler_args);
      self->handle_mqtt_event(base, event_id, event_data);
   }

   void handle_event_data(esp_mqtt_event_handle_t event)
   {
      std::string topic(event->topic, event->topic_len);
      std::string data(event->data, event->data_len);

      printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
      printf("DATA=%.*s\r\n", event->data_len, event->data);

      auto handles = message_handlers.equal_range(topic);

      std::for_each(handles.first, handles.second, [&data](const auto &pair) {
         auto &[topic, handler] = pair;
         printf("Calling handler for topic %s\n", topic.data());
         handler(topic, data);
      });
   }

   void handle_mqtt_event(esp_event_base_t base, int32_t event_id, void *event_data)
   {
      switch (event_id) {
      case esp_mqtt_event_id_t::MQTT_EVENT_CONNECTED:
         ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
         resubscribe_all();
         break;
      case esp_mqtt_event_id_t::MQTT_EVENT_DISCONNECTED:
         ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
         break;
      case esp_mqtt_event_id_t::MQTT_EVENT_SUBSCRIBED:
         ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
         break;
      case esp_mqtt_event_id_t::MQTT_EVENT_UNSUBSCRIBED:
         ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
         break;
      case esp_mqtt_event_id_t::MQTT_EVENT_PUBLISHED:
         ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
         break;
      case esp_mqtt_event_id_t::MQTT_EVENT_DATA: {
         ESP_LOGI(TAG, "MQTT_EVENT_DATA");
         auto event = static_cast<esp_mqtt_event_handle_t>(event_data);
         handle_event_data(event);
         break;
      }
      case esp_mqtt_event_id_t::MQTT_EVENT_ERROR:
         ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
         break;
      default:
         ESP_LOGI(TAG, "Other event id:%d", event_id);
         break;
      }
   }

   void subscribe(const std::string &topic, MessageHandler_t handler)
   {
      message_handlers.emplace(topic, handler);
      esp_mqtt_client_subscribe(m_client, topic.data(), 0);
      ESP_LOGI(TAG, "Subscribed to topic: %s", topic.data());
   }

   void resubscribe_all()
   {
      auto keys = message_handlers | std::ranges::views::keys | std::ranges::to<std::set<std::string>>();
      for(const auto &topic : keys) {
         const auto msg_id = esp_mqtt_client_subscribe(m_client, topic.data(), 0);
         ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
      }
   }

   void init()
   {
      esp_mqtt_client_config_t mqtt_cfg = {};
      mqtt_cfg.broker.address.hostname = "192.168.0.235";
      mqtt_cfg.broker.address.port = 1883;
      mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;

      m_client = esp_mqtt_client_init(&mqtt_cfg);
      assert(m_client != nullptr && "Failed to create MQTT client");

      ESP_ERROR_CHECK(esp_mqtt_client_register_event(
          m_client,
          esp_mqtt_event_id_t::MQTT_EVENT_ANY,
          &MqttClient::mqtt_event_handler_static,
          this));

      ESP_ERROR_CHECK(esp_mqtt_client_start(m_client));
   }
};