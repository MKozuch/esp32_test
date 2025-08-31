/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "driver/gpio.h"

#include "esp_err.h"
#include "esp_event_base.h"
#include "esp_netif_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "hal/gpio_types.h"
#include "driver/gpio.h"

#include "esp_wifi.h"
#include "esp_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "portmacro.h"
#include "mqtt_client.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <deque>
#include <esp_log.h>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "WifiClient.hpp"
#include "MqttClient.hpp"


constexpr auto LED_BLUE_PIN = gpio_num_t::GPIO_NUM_4;
constexpr auto LED_GREEN_PIN = gpio_num_t::GPIO_NUM_5;
constexpr auto LED_RED_PIN = gpio_num_t::GPIO_NUM_6;
constexpr auto BUTTON_PIN = gpio_num_t::GPIO_NUM_0;

constexpr auto TAG = "asdf";

enum class LogicLevel : bool {
   LOW = false,
   HIGH = true,
};

enum class LedColor : int {
   BLUE = gpio_num_t::GPIO_NUM_4,
   GREEN = gpio_num_t::GPIO_NUM_5,
   RED = gpio_num_t::GPIO_NUM_6,
};

struct QueueMessagePayload {
   LogicLevel buttonState;
   LedColor ledColor;
};

std::deque<QueueMessagePayload> fifo;

std::mutex mtx;

class ButtonMonitorTask {
 public:
   ButtonMonitorTask(QueueHandle_t queue) : queue_(queue)
   {
      assert(queue_ != nullptr && "Queue handle is null");
   }

   static void public_run(void *arg)
   {
      assert(arg != nullptr && "ButtonMonitorTask instance is null");

      auto self = static_cast<ButtonMonitorTask *>(arg);
      self->run();
   }

 private:
   void run()
   {
      std::cout << "Hello world from btn monitor!" << std::endl;

      while (true) {
         {
            std::lock_guard lock(mtx);
            const auto button_state = gpio_get_level(BUTTON_PIN);
            QueueMessagePayload payload{.buttonState = static_cast<LogicLevel>(button_state), .ledColor = LedColor::GREEN};
            fifo.push_back(payload);
         }
         std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
   }

   QueueHandle_t queue_ = nullptr;
};

class LedControllerTask {
 public:
   LedControllerTask(QueueHandle_t queue) : queue_(queue)
   {
      assert(queue_ != nullptr && "Queue handle is null");
   }

   static void public_run(void *arg)
   {
      assert(arg != nullptr && "LedControllerTask instance is null");

      auto self = static_cast<LedControllerTask *>(arg);
      self->run();
   }

 private:
   void run()
   {
      std::cout << "Hello world from LED controller!" << std::endl;

      while (true) {
         {
            std::lock_guard lock(mtx);
            if (not fifo.empty()) {
               const auto &payload = fifo.front();
               gpio_set_level(static_cast<gpio_num_t>(payload.ledColor), static_cast<int>(payload.buttonState));
               fifo.pop_front();
            }
         }

         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
   }

   QueueHandle_t queue_ = nullptr;
};





extern "C" {
void app_main(void)
{
   // config output pins
   for (auto pin : {LED_BLUE_PIN, LED_GREEN_PIN, LED_RED_PIN}) {
      gpio_reset_pin(pin);
      gpio_set_direction(pin, GPIO_MODE_OUTPUT);
      gpio_set_level(pin, 1);
   }

   // config input pin
   gpio_reset_pin(BUTTON_PIN);
   gpio_set_direction(BUTTON_PIN, gpio_mode_t::GPIO_MODE_INPUT);
   gpio_set_pull_mode(BUTTON_PIN, gpio_pull_mode_t::GPIO_FLOATING);

   const auto queue1 = xQueueCreate(100, sizeof(int));
   assert(queue1 != nullptr && "Failed to create queue");

   TaskHandle_t task_handle = nullptr;

   const size_t stack_size = 1024;

   // setup tasks
   // auto buttonMonitorTask = ButtonMonitorTask(queue1);
   // auto ledControllerTask = LedControllerTask(queue1);

   //    xTaskCreate(
   //        &ButtonMonitorTask::public_run,
   //        "MonitorTask",
   //        stack_size,
   //        &buttonMonitorTask,
   //        10,
   //        &task_handle);
   //    assert(task_handle != nullptr && "Failed to create button monitor task");

   // xTaskCreate(
   //     &LedControllerTask::public_run,
   //     "LedTask",
   //     stack_size,
   //     &ledControllerTask,
   //     10,
   //     &task_handle);
   // assert(task_handle != nullptr && "Failed to create LED controller task");

   auto wifi = WifiClient::get_instance();
   wifi->init();

   auto mqtt = MqttClient{};

   auto init_mqtt_handler = [](void *event_handler_arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data) {
      if (event_base == IP_EVENT and event_id == IP_EVENT_STA_GOT_IP) {
         auto mqtt = static_cast<MqttClient *>(event_handler_arg);
         mqtt->init();
      }
   };

   ESP_ERROR_CHECK(esp_event_handler_instance_register(
       IP_EVENT,
       IP_EVENT_STA_GOT_IP,
       init_mqtt_handler,
       &mqtt,
       nullptr));


   auto led_mqtt_handler = [](const std::string &topic, const std::string &data) {
        if (topic == "led1") {
           const bool is_on = (data == "1" or data == "on");
           gpio_set_level(LED_RED_PIN, is_on ? 0 : 1);
           

           // TODO: echo
           //static uint8_t cnt = 0;
           //++cnt;
           //const auto str = std::to_string(cnt);
           //esp_mqtt_client_publish(client, "gauge", str.data(), str.size(), 0, 0);
        }
   };

   mqtt.subscribe("led1", led_mqtt_handler);

   while (true) {
      vTaskDelay(2000 / portTICK_PERIOD_MS);

      // TODO: soft restart on button to make sure order of initialization is correct
      // const auto button_state = static_cast<LogicLevel>(gpio_get_level(BUTTON_PIN));
      // std::cout << "Button state: " << (button_state == LogicLevel::HIGH ? "HIGH" : "LOW") << std::endl;
      std::cout << "Main thread ping" << std::endl;
      wifi->log_status();
   }
}
} // extern "C"