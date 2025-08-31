#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Define a FreeRTOS task
void my_hello_task(void *pvParameter) {
    ESP_LOGI("HelloTask", "Starting the Hello World task...");
    
    // An RTOS task should not return. It should run indefinitely or delete itself.
    while (1) {
        std::cout << "Hello, World from C++!" << std::endl;
        // Delay the task for 1 second.
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// The main entry point for the ESP-IDF application.
extern "C" void app_main(void) {
    ESP_LOGI("AppMain", "Creating the Hello World task.");
    
    // Create the task.
    xTaskCreate(
        my_hello_task,      // Task function
        "HelloTask",        // Name of the task
        2048,               // Stack size in words (not bytes!)
        NULL,               // Parameter to pass to the task
        5,                  // Task priority
        NULL                // Handle to the created task
    );
}
