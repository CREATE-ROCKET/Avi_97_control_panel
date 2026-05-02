#pragma once

#include <Arduino.h>
#include <freertos/semphr.h>
#include "CANCREATE.h"

// Pin definitions
constexpr uint8_t DUMP_PIN = 16;
constexpr uint8_t SEPARATE_PIN = 17;
constexpr uint8_t FIRE_PIN = 18;
constexpr uint8_t VALVESET_PIN = 19;
constexpr uint8_t FILL_PIN = 21;
constexpr uint8_t O2_PIN = 34;
// constexpr uint8_t MAIN_RESET_PIN = 35;
constexpr uint8_t VALVE_OPEN_PIN = 35;

constexpr uint8_t MCU_LUMP_PIN = 25;
constexpr uint8_t CAN_TX_PIN = 26;
constexpr uint8_t CAN_RX_PIN = 27;
constexpr uint8_t SERIAL1_RX_PIN = 32;
constexpr uint8_t SERIAL1_TX_PIN = 33;
// constexpr uint8_t TFT_CS = 23;
// constexpr uint8_t TFT_DC = 13;
// constexpr uint8_t TFT_RST = 22;
// constexpr uint8_t TFT_SCK = 14;
// constexpr int8_t TFT_MISO = -1;
// constexpr uint8_t TFT_MOSI = 4;

// CAN IDs
constexpr uint32_t CAN_ID_BUTTON_STATE = 0x101;
constexpr uint32_t CAN_ID_MAIN_VALVE_ANGLE = 0x102;
constexpr uint32_t CAN_ID_MAIN_STATE = 0x103;
constexpr uint32_t CAN_ID_MAIN_VALVE_STATE = 0x107;

// Timing and protocol constants
constexpr long COMMUNICATION_TIMEOUT_MS = 3000;
constexpr long ERROR_COMMUNICATION_TIMEOUT_MS = 300;
constexpr int SAMPLING_RATE_MS = 8;

constexpr int16_t OPEN_ANGLE = -35;
constexpr int16_t CLOSE_ANGLE = 55;
constexpr int ANGLE_CAN_SIZE = 2;
constexpr int16_t ANGLE_SCALE = 10;
constexpr int16_t ANGLE_STATUS_TOLERANCE_DEG = 20;

extern uint8_t buttonState;
extern uint8_t mainState;
extern uint8_t valveState;
extern int16_t valveAngleX10;

extern unsigned long lastMainRx;
extern unsigned long lastValveRx;

extern SemaphoreHandle_t stateMutex;
extern CAN_CREATE CAN;
