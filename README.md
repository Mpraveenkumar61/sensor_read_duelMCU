# Dual-MCU RTOS Sensor Bridge
### STM32 & ESP32 Bi-Directional Data Pipeline

> A robust, multi-threaded embedded system using FreeRTOS to bridge analog sensor data from an ESP32 to an STM32F103 for real-time visualization on an SSD1306 OLED display via UART.

---

## Table of Contents
- [Project Architecture](#1-project-architecture)
- [Technical Challenges & Solutions](#2-technical-challenges--solutions)
- [Communication Protocol](#3-communication-protocol)
- [Hardware Requirements](#4-hardware-requirements)
- [Software Stack](#5-software-stack)
- [How to Build](#how-to-build)

---

## 1. Project Architecture

The system is divided into two distinct processing nodes communicating over a **115200-baud serial link**. Instead of hardware interrupts, the design uses **High-Priority RTOS Tasks** and **Thread-Safe Queues** to manage data flow.

```
┌─────────────────────────────┐        UART 115200       ┌──────────────────────────────┐
│        Node A: ESP32        │ ──────────────────────►  │       Node B: STM32          │
│        (Producer)           │    GPIO17 TX → PA10 RX   │       (Consumer)             │
│                             │                           │                              │
│  • ADC Sampling Task        │                           │  • UART RX Task (high prio)  │
│  • UART TX Task             │                           │  • OLED Display Task         │
│  • 16-sample burst average  │                           │  • SSD1306 I2C Rendering     │
│  • Dead-Band Filter         │                           │  • State Machine Parser      │
└─────────────────────────────┘                           └──────────────────────────────┘
                                                                        │
                                                                        ▼
                                                             ┌─────────────────┐
                                                             │  SSD1306 OLED   │
                                                             │  128x64 Display │
                                                             └─────────────────┘
```

### Node A: ESP32 (The Producer)

| Task | Description |
|---|---|
| **ADC Sampling** | Samples a potentiometer at 12-bit resolution with a 16-sample burst average and Dead-Band filter to eliminate signal jitter |
| **UART TX** | Monitors a local queue; on significant change, formats data as `%lu\n` and transmits |

> A **50ms Cool-Down** delay prevents overwhelming the STM32's I2C bus.

### Node B: STM32 (The Consumer)

| Task | Description |
|---|---|
| **UART RX** | High-priority polling task with 1ms timeout. Uses a **State Machine** to identify numeric characters; resets buffer on `\n` or `\r` |
| **OLED Display** | Consumes data from `uart_to_oled_queue`; manages I2C protocol for SSD1306, rendering sensor value and status messages |

---

## 2. Technical Challenges & Solutions

### 🐛 The "Number Mashing" Bug

**Problem:** The STM32 occasionally displayed massive 7-digit numbers (e.g., `1811727`) instead of the actual 4-digit sensor value.

**Root Cause:** The OLED task took ~35ms to update the screen via I2C. During this blocked window, the UART hardware buffer overflowed, causing the STM32 to miss the Newline (`\n`) character — making two consecutive numbers merge into one string.

**Fix — Three-Pronged Solution:**

```
1. Strict Digit Filtering   →  UART task discards all non-numeric characters
2. Zero-Footprint Polling   →  Optimized task loop using vTaskDelay(1) to maximize listen time
3. Producer Throttling      →  50ms delay on ESP32 matches STM32's max real-time refresh rate
```

---

## 3. Communication Protocol

ASCII text encoding is used for human-readability during debugging and ease of parsing on the STM32.

| Parameter   | Value         |
|-------------|---------------|
| Baud Rate   | 115200        |
| Data Bits   | 8             |
| Parity      | None          |
| Stop Bits   | 1             |
| Terminator  | `\n` (LF)     |

---

## 4. Hardware Requirements

| Component | Part |
|---|---|
| MCU A | ESP32-WROOM-32 |
| MCU B |STM32F103RB  (NUCLEO)  | |
| Display | SSD1306 128×64 I2C OLED |
| Sensor | 10kΩ Potentiometer |

### Wiring

```
ESP32  GPIO17 (TX)  ──────────────►  PA10 / UART1 (RX)  STM32
ESP32  GND          ──────────────►  GND                 STM32
```

> ⚠️ **Common Ground (GND) is mandatory** for signal integrity.

---

## 5. Software Stack

| Node | Tools & Libraries |
|---|---|
| **STM32** | STM32CubeIDE, HAL Drivers, FreeRTOS CMSIS-RTOS V1 |
| **ESP32** | ESP-IDF (VS Code Extension), FreeRTOS Native API |

---

## How to Build

### ESP32
```bash
# Open the main/ folder in VS Code with the ESP-IDF extension
idf.py build flash
```

### STM32
1. Open the project in **STM32CubeIDE**
2. Ensure `string.h` and `stdlib.h` are included in `freertos.c`
3. **Compile** and **Flash** via ST-Link

---

## License

This project is open-source and available under the [MIT License](LICENSE).
