/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "button_gpio.h"


#include "iot_button.h"
#include "portmacro.h"

#include <cassert>
#include <chrono>
#include <deque>
#include <esp_log.h>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>


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

void on_button_short_press(void *arg,void *usr_data)
{
   std::cout << "Button short press detected!" << std::endl;
   static int press_count = 0;
   {
      std::lock_guard lock(mtx);
      press_count++;

      LogicLevel button_state = (press_count % 2 == 0) ? LogicLevel::LOW : LogicLevel::HIGH;

      fifo.push_back({.buttonState = button_state, .ledColor = LedColor::GREEN});
   }
}

void on_button_long_press_down(void *arg,void *usr_data)
{
   std::cout << "Button long press detected!" << std::endl;

   {
      std::lock_guard lock(mtx);
      fifo.push_back({.buttonState = LogicLevel::HIGH, .ledColor = LedColor::BLUE});
   }
}

void on_button_long_press_up(void *arg, void *usr_data)
{
   std::cout << "Button long press released!" << std::endl;

   {
      std::lock_guard lock(mtx);
      fifo.push_back({.buttonState = LogicLevel::LOW, .ledColor = LedColor::BLUE});
   }
}

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

   // setup button (TODO: wrap with builder)
   const button_config_t btn_cfg = {.long_press_time = 500, .short_press_time = 10};
   const button_gpio_config_t btn_gpio_cfg = {
       .gpio_num = 0,
       .active_level = 0,
       .enable_power_save = false,
       .disable_pull = true};

   button_handle_t gpio_btn = nullptr;
   esp_err_t ret = ESP_OK;
   ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &gpio_btn);
   assert(ret == ESP_OK && "Failed to create GPIO button");

   ret = iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, nullptr, on_button_short_press, nullptr);
   assert(ret == ESP_OK && "Failed to register short press callback");
   ret = iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_START, nullptr, on_button_long_press_down, nullptr);
   assert(ret == ESP_OK && "Failed to register long press callback");
   ret = iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_UP, nullptr, on_button_long_press_up, nullptr);
   assert(ret == ESP_OK && "Failed to register long press up callback");

   // setup tasks
   // auto buttonMonitorTask = ButtonMonitorTask(queue1);
   auto ledControllerTask = LedControllerTask(queue1);

   //    xTaskCreate(
   //        &ButtonMonitorTask::public_run,
   //        "MonitorTask",
   //        stack_size,
   //        &buttonMonitorTask,
   //        10,
   //        &task_handle);
   //    assert(task_handle != nullptr && "Failed to create button monitor task");

   xTaskCreate(
       &LedControllerTask::public_run,
       "LedTask",
       stack_size,
       &ledControllerTask,
       10,
       &task_handle);
   assert(task_handle != nullptr && "Failed to create LED controller task");

   while (true) {
      vTaskDelay(2000 / portTICK_PERIOD_MS);

      // const auto button_state = static_cast<LogicLevel>(gpio_get_level(BUTTON_PIN));
      // std::cout << "Button state: " << (button_state == LogicLevel::HIGH ? "HIGH" : "LOW") << std::endl;
      std::cout << "Main thread ping" << std::endl;
   }
}
}
