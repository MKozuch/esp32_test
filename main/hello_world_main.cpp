/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <cinttypes>
#include <cstdio>
#include <esp_log.h>
#include <iostream>
#include "driver/uart.h"
#include <thread>
#include <chrono>

constexpr auto LED_BLUE_PIN = gpio_num_t::GPIO_NUM_4;
constexpr auto LED_GREEN_PIN = gpio_num_t::GPIO_NUM_5;
constexpr auto LED_RED_PIN = gpio_num_t::GPIO_NUM_6;

constexpr auto TAG = "asdf";

[[noreturn]] void task1(void *args)
{
    std::ignore = args;

    while (true)
    {
        std::cout << "Hello world!" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

[[noreturn]] void task2(void *args)
{
    std::ignore = args;

    while (true)
    {
        std::cout << "Hello world!" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

extern "C" {
[[noreturn]] void app_main(void)
    {
        gpio_reset_pin(LED_RED_PIN);
        gpio_set_direction(LED_RED_PIN, GPIO_MODE_OUTPUT);

        TaskHandle_t xHandle = nullptr;

        xTaskCreate(
            task1,
            "HelloTask",
            1024,
            nullptr,
            10,
            &xHandle
        );

        while(true)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            std::cout << "Main thread ping" << std::endl;
        }
    }
}
