#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdlib.h>

/* Peripheral Handles */
I2C_HandleTypeDef  hi2c1;
UART_HandleTypeDef huart1; // Communication with ESP32
UART_HandleTypeDef huart2; // Debug Logs to PC (PuTTY)

/* RTOS Handles */
QueueHandle_t uart_to_oled_queue = NULL;

/* Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

/* Link Tasks from freertos.c or elsewhere */
extern void StartTask3_UARTReceive(void const *argument);
extern void StartTask4_OLEDDisplay(void const *argument);

/* printf Redirect to UART2 */
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 10);
    return ch;
}

/**
  * @brief  The application entry point.
  */
int main(void) {
    /* MCU Configuration */
    HAL_Init();

    /* Using Internal Clock (HSI) for stability during debugging */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART2_UART_Init(); // Debug UART first
    MX_I2C1_Init();
    MX_USART1_UART_Init();

    printf("\r\n[SYSTEM] Booting Safe Mode...\r\n");

    /* --- I2C Bus Scanner --- */
    printf("--- I2C Scan Start ---\r\n");
    for (uint16_t i = 0; i < 128; i++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(i << 1), 3, 5) == HAL_OK) {
            printf("SUCCESS: Found device at 7-bit address 0x%02X\r\n", i);
        }
    }
    printf("--- I2C Scan End ---\r\n");

    /* 1. Create the Queue (Capacity: 5 integers) */
    uart_to_oled_queue = xQueueCreate(5, sizeof(uint32_t));

    if (uart_to_oled_queue != NULL) {
        printf("[RTOS] Queue Created Successfully.\r\n");

        /* 2. Create Task 3: UART Receive (Stack: 128 words) */
        osThreadDef(Task3_UART, StartTask3_UARTReceive, osPriorityNormal, 0, 128);
        osThreadId uartHandle = osThreadCreate(osThread(Task3_UART), NULL);
        if(uartHandle == NULL) {
            printf("[ERROR] UART Task Allocation Failed!\r\n");
        } else {
            printf("[RTOS] UART Task Created.\r\n");
        }

        /* 3. Create Task 4: OLED Display (Stack: 384 words for snprintf) */
        osThreadDef(Task4_OLED, StartTask4_OLEDDisplay, osPriorityNormal, 0, 384);
        osThreadId oledHandle = osThreadCreate(osThread(Task4_OLED), NULL);
        if(oledHandle == NULL) {
            printf("[ERROR] OLED Task Allocation Failed!\r\n");
        } else {
            printf("[RTOS] OLED Task Created.\r\n");
        }

        /* 4. Start the RTOS Scheduler */
        printf("[MAIN] Starting Scheduler...\r\n");
        osKernelStart();
    } else {
        printf("[CRITICAL] Queue Creation Failed! RAM is full.\r\n");
    }

    /* We should never reach here if the scheduler starts */
    while (1) {
        // Fallback if scheduler fails
    }
}

/* --- I2C1 Initialization --- */
static void MX_I2C1_Init(void) {
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        // We don't printf here yet because UART might not be ready
        Error_Handler();
    }
}

/* --- UART1 Initialization (ESP32) --- */
static void MX_USART1_UART_Init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

/* --- UART2 Initialization (Debug PC) --- */
static void MX_USART2_UART_Init(void) {
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

/* --- GPIO Initialization --- */
static void MX_GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
}

/* --- Internal Clock Configuration --- */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

void Error_Handler(void) {
    __disable_irq();
    while (1);
}
