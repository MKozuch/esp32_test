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
#include "portmacro.h"

#include <cassert>
#include <chrono>
#include <esp_log.h>
#include <initializer_list>
#include <iostream>
#include <memory>
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

struct QueueMessagePayload {
    LogicLevel buttonState = LogicLevel::LOW;
};

[[noreturn]] void buttonMonitorTask(QueueHandle_t queue)
{
    assert(queue != nullptr && "Queue handle is null");

    std::cout << "Hello world from btn monitor!" << std::endl;

    while (true) {
        const auto button_state = static_cast<LogicLevel>(gpio_get_level(BUTTON_PIN));
        ESP_LOGI(TAG, "Button state: %d", static_cast<int>(button_state));

        auto payload = QueueMessagePayload{.buttonState = button_state};
        xQueueSend(queue, static_cast<void *>(&payload), TickType_t(0));

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

[[noreturn]] void ledControllerTask(QueueHandle_t queue)
{
    assert(queue != nullptr && "Queue handle is null");

    ESP_LOGI(TAG, "Hello world from LED controller task!");

    QueueMessagePayload payload;

    while (true) {
        if (xQueueReceive(queue, &(payload), (TickType_t)10000)) {
            ESP_LOGI(TAG, "Received message from queue: buttonChanged=%d", payload.buttonState);

            gpio_set_level(LED_RED_PIN, static_cast<int>(payload.buttonState));
        }
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
    gpio_set_pull_mode(BUTTON_PIN, gpio_pull_mode_t::GPIO_PULLUP_ONLY);

    const auto queue1 = xQueueCreate(100, sizeof(QueueMessagePayload));
    assert(queue1 != nullptr && "Failed to create queue");

    TaskHandle_t task_handle = nullptr;

    const size_t stack_size = 64;

    xTaskCreate(
        [](void *ptr) { buttonMonitorTask(static_cast<QueueHandle_t>(ptr)); },
        "MonitorTask", 
        stack_size, 
        queue1, 
        10, 
        &task_handle);
    assert(task_handle != nullptr && "Failed to create button monitor task");

    xTaskCreate(
        [](void *ptr) { ledControllerTask(static_cast<QueueHandle_t>(ptr)); },
        "LedTask", 
        stack_size, 
        queue1, 
        9, 
        &task_handle);
    assert(task_handle != nullptr && "Failed to create LED controller task");

    while (true) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        //std::cout << "Main thread ping" << std::endl;
    }
}
}
