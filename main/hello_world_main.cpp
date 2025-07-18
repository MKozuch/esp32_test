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
#include <initializer_list>

constexpr auto LED_BLUE_PIN = gpio_num_t::GPIO_NUM_4;
constexpr auto LED_GREEN_PIN = gpio_num_t::GPIO_NUM_5;
constexpr auto LED_RED_PIN = gpio_num_t::GPIO_NUM_6;

constexpr auto TAG = "asdf";


[[noreturn]] void task1(void *args)
{
    std::ignore = args;

    while (true)
    {
        gpio_set_level(LED_RED_PIN, 1);
        std::cout << "Hello world from task 1!" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        gpio_set_level(LED_RED_PIN, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

[[noreturn]] void task2(void *args)
{
    std::ignore = args;

    while (true)
    {
        gpio_set_level(LED_BLUE_PIN, 1);
        std::cout << "Hello world from task 2!" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        gpio_set_level(LED_BLUE_PIN, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void task3(){
    while(true){
        std::cout << "Hello from task 3" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        gpio_set_level(LED_GREEN_PIN, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        gpio_set_level(LED_GREEN_PIN, 1);
    }
}


extern "C" {
//[[noreturn]]
 void app_main(void)
    {
        for(auto pin : {LED_BLUE_PIN, LED_GREEN_PIN, LED_RED_PIN}){
            gpio_reset_pin(pin);
            gpio_set_direction(pin, GPIO_MODE_OUTPUT);
            gpio_set_level(pin, 0);
        }
        
        TaskHandle_t xHandle = nullptr;

        xTaskCreate(
            task1,
            "HelloTask2",
            1024,
            nullptr,
            10,
            &xHandle
        );

        xTaskCreate(
            task2,
            "HelloTask2",
            1024,
            nullptr,
            10,
            &xHandle
        );

        auto t = std::thread(&task3);

       while(true)
       {
           vTaskDelay(5000 / portTICK_PERIOD_MS);
           std::cout << "Main thread ping" << std::endl;
       }
  }
}
