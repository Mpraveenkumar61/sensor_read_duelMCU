#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "main.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* External handles from main.c */
extern UART_HandleTypeDef huart1;
extern QueueHandle_t uart_to_oled_queue;

/**
 * TASK 3: UART RECEIVE
 * Receives ASCII data from ESP32, converts to integer, and sends to Queue.
 */
void StartTask3_UARTReceive(void const *argument) {
    char frame_buf[16];
    uint8_t idx = 0;
    uint8_t byte;

    memset(frame_buf, 0, sizeof(frame_buf));
    printf("[UART] Task Started. Clean Sync Mode...\r\n");

    for (;;) {
        // 1. We wait indefinitely (or 10ms) for a single byte
        if (HAL_UART_Receive(&huart1, &byte, 1, 10) == HAL_OK) {

            // 2. If we hit a terminator, process immediately
            if (byte == '\n' || byte == '\r') {
                if (idx > 0) {
                    frame_buf[idx] = '\0';
                    uint32_t val = (uint32_t)atoi(frame_buf);

                    // Only send valid data to OLED
                    xQueueSend(uart_to_oled_queue, &val, 0);

                    idx = 0; // Reset for next number
                    // Do NOT delay here anymore; we want to be ready for the next byte
                }
            }
            // 3. Only accept numeric characters to prevent "mushing"
            else if (byte >= '0' && byte <= '9') {
                if (idx < sizeof(frame_buf) - 1) {
                    frame_buf[idx++] = byte;
                }
            }
            // 4. If we see anything else (like noise), we treat it as a reset
            else {
                idx = 0;
            }
        } else {
            // No data? Yield to other tasks
        	vTaskDelay(1);        }
    }
}

/**
 * TASK 4: OLED DISPLAY
 * Consumes data from Queue and renders it to the SSD1306.
 */
void StartTask4_OLEDDisplay(void const *argument) {
    uint32_t received_val = 0;
    char str[16];

    vTaskDelay(pdMS_TO_TICKS(1000));

        // Force a "Bus Clear" - Sometimes I2C gets stuck in a BUSY state
    __HAL_I2C_DISABLE(&hi2c1);
    HAL_Delay(10);
    __HAL_I2C_ENABLE(&hi2c1);

    printf("[OLED] Attempting Init...\r\n");
    ssd1306_Init();
    printf("[OLED] Init Passed!\r\n"); // <-- We need to reach this line!

    for (;;) {
        // Increase timeout to 2 seconds to be very patient
        if (xQueueReceive(uart_to_oled_queue, &received_val, pdMS_TO_TICKS(2000)) == pdPASS) {
            printf("[OLED] Processing: %lu\r\n", (unsigned long)received_val);

            snprintf(str, sizeof(str), "%lu", (unsigned long)received_val);

            ssd1306_Fill(Black);
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("pot value:", Font_11x18, White);
            ssd1306_SetCursor(0, 30);
            ssd1306_WriteString(str, Font_11x18, White);
            ssd1306_UpdateScreen();
        } else {
            printf("[OLED] Waiting for data...\r\n");
        }
    }
}


/* FreeRTOS Static Allocation Helpers */

/* --- FreeRTOS Static Allocation Helpers --- */

/* * vApplicationGetIdleTaskMemory: Required when configSUPPORT_STATIC_ALLOCATION is 1.
 * This provides the memory for the Idle Task which runs when no other tasks are ready.
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCBBuffer;
    /* configMINIMAL_STACK_SIZE is usually 128 words */
    static StackType_t  xIdleStack[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer   = &xIdleTaskTCBBuffer;
    *ppxIdleTaskStackBuffer = &xIdleStack[0];
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

/* * NOTE: vApplicationGetTimerTaskMemory was removed because
 * configUSE_TIMERS is likely set to 0 in your FreeRTOSConfig.h.
 */
