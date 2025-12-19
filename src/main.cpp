#include <Arduino.h>
#include "CANCREATE.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// --- Pin Definitions --- 12と13も使えるかも(separation に使う可能性)
constexpr uint8_t FILL_PIN = 16;
constexpr uint8_t VALVESET_PIN = 4;
constexpr uint8_t DUMP_PIN = 34;
constexpr uint8_t FIRE_PIN = 35;
constexpr uint8_t FD_PIN = 17;
constexpr uint8_t MCU_LUMP_PIN = 15;
constexpr uint8_t CAN_TX_PIN = 32;
constexpr uint8_t CAN_RX_PIN = 33;
constexpr uint8_t SERIAL1_RX_PIN = 21;
constexpr uint8_t SERIAL1_TX_PIN = 18;

// --- CAN ID Definitions ---
constexpr uint32_t CAN_ID_BUTTON_STATE = 0x101;
constexpr uint32_t CAN_ID_MAIN_VALVE_ANGLE = 0x102;
constexpr uint32_t CAN_ID_FROM_PLC_ACK = 0x103;
constexpr uint32_t CAN_ID_TO_PLC_ACK = 0x104;

// --- Constants ---
constexpr long PLC_TIMEOUT_MS = 3000;
constexpr int DEBOUNCE_DELAY_MS = 20;

// --- Enums for State Management ---
enum PLCStatus
{
  PLC_OK,
  PLC_DEAD
};
enum ButtonState
{
  RELEASED,
  WAITING_DEBOUNCE,
  PRESSED
};

// --- Global State Variables & Mutexes ---
PLCStatus plcStatus = PLC_OK;
ButtonState fireButtonState = RELEASED;
ButtonState FDButtonState = RELEASED;

bool isFireButtonPressed = false;
bool isfdPressed = false;

unsigned long long lastPLCACK = 0;
unsigned long long fireDebounceTimer = 0;
unsigned long long fdDebounceTimer = 0;

SemaphoreHandle_t plcStatusMutex;
SemaphoreHandle_t buttonStateMutex;

CAN_CREATE CAN(true);
// --- Task & Function Prototypes ---
void CANRecvTask(void *pvParameters);
void CANSendTask(void *pvParameters);
void updatePLCStatus();
void updateButtonState(ButtonState &currentState, bool &isPressed, unsigned long long &debounceTimer, const uint8_t pin);

void setup()
{
  pinMode(MCU_LUMP_PIN, OUTPUT);
  pinMode(FILL_PIN, INPUT);
  pinMode(VALVESET_PIN, INPUT);
  pinMode(DUMP_PIN, INPUT);
  pinMode(FIRE_PIN, INPUT);
  pinMode(FD_PIN, INPUT);

  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, SERIAL1_RX_PIN, SERIAL1_TX_PIN);

  plcStatusMutex = xSemaphoreCreateMutex();
  buttonStateMutex = xSemaphoreCreateMutex();

  Serial.println("CAN Control Panel");
  if (CAN.begin(100E3, CAN_RX_PIN, CAN_TX_PIN))
  {
    while (1)
      ;
  }
  delay(1000);
  xTaskCreateUniversal(CANRecvTask, "CANRecvTask", 2048, NULL, 0, NULL, APP_CPU_NUM);
  xTaskCreateUniversal(CANSendTask, "CANSendTask", 2048, NULL, 0, NULL, APP_CPU_NUM);
  // switch (CAN.test())
  // {
  // case CAN_SUCCESS:
  //   Serial.println("Success!!!");
  //   break;
  // case CAN_UNKNOWN_ERROR:
  //   Serial.println("Unknown error occurred");
  //   break;
  // case CAN_NO_RESPONSE_ERROR:
  //   Serial.println("No response error");
  //   break;
  // case CAN_CONTROLLER_ERROR:
  //   Serial.println("CAN CONTROLLER ERROR");
  //   break;
  // default:
  //   break;
  // }
}

void loop()
{
  updatePLCStatus();
  updateButtonState(fireButtonState, isFireButtonPressed, fireDebounceTimer, FIRE_PIN);
  updateButtonState(FDButtonState, isfdPressed, fdDebounceTimer, FD_PIN);
  delay(100);
}

void updatePLCStatus()
{
  static short blink_count = 0;
  xSemaphoreTake(plcStatusMutex, portMAX_DELAY);
  if (millis() - lastPLCACK < PLC_TIMEOUT_MS)
  {
    plcStatus = PLC_OK;
  }
  else
  {
    plcStatus = PLC_DEAD;
  }
  xSemaphoreGive(plcStatusMutex);
  switch (plcStatus)
  {
  case PLC_OK:
    digitalWrite(MCU_LUMP_PIN, HIGH);
    blink_count = 0;
    break;
  case PLC_DEAD:
    // Blink the LED to indicate PLC is dead
    digitalWrite(MCU_LUMP_PIN, (blink_count++ % 20 < 10) ? HIGH : LOW);
    break;
  }
}

void updateButtonState(ButtonState &currentState, bool &isPressed, unsigned long long &debounceTimer, const uint8_t pin)
{
  bool reading = digitalRead(pin);

  xSemaphoreTake(buttonStateMutex, portMAX_DELAY);
  switch (currentState)
  {
  case RELEASED:
    if (reading)
    {
      debounceTimer = millis();
      currentState = WAITING_DEBOUNCE;
    }
    isPressed = false;
    break;
  case WAITING_DEBOUNCE:
    if (!reading)
    {
      currentState = RELEASED;
    }
    else if (millis() - debounceTimer > DEBOUNCE_DELAY_MS)
    {
      currentState = PRESSED;
      isPressed = true;
    }
    break;
  case PRESSED:
    if (!reading)
    {
      currentState = RELEASED;
    }
    // isPressed remains true while the button is physically held down
    break;
  }
  xSemaphoreGive(buttonStateMutex);
}

void CANRecvTask(void *pvParameters)
{
  while (1)
  {
    if (CAN.available())
    {
      can_return_t message;
      if (!CAN.readWithDetail(&message))
      {
        switch (message.id)
        {
        case CAN_ID_MAIN_VALVE_ANGLE:
        {
          short rdata = message.data[0]-120;
          Serial1.print("rdata: ");
          Serial1.println(rdata);
          float angle = rdata * 8000 / 270 + 7000;
          angle = 135 * (angle - 7500) / 4000;
          Serial1.print("MainAngle: ");
          Serial1.println(angle);
        }
        case CAN_ID_FROM_PLC_ACK:
        {
          xSemaphoreTake(plcStatusMutex, portMAX_DELAY);
          lastPLCACK = millis();
          xSemaphoreGive(plcStatusMutex);
          break;
        }
        default:
        {
          break;
        }
        }
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void CANSendTask(void *pvParameters)
{
  while (1)
  {
    uint8_t data = 0;
    uint8_t ack = 0;
    CAN.sendData(CAN_ID_TO_PLC_ACK, &ack, 1);
    bool firePressed, fdPressed;
    xSemaphoreTake(buttonStateMutex, portMAX_DELAY);
    firePressed = isFireButtonPressed;
    fdPressed = isfdPressed;
    xSemaphoreGive(buttonStateMutex);

    data |= (digitalRead(DUMP_PIN) & 1) << 0;
    data |= (digitalRead(FILL_PIN) & 1) << 1;
    data |= (firePressed && !digitalRead(FD_PIN) && !digitalRead(FILL_PIN)) << 2; // FD押下中はFire無効
    data |= (fdPressed) << 3;
    data |= (digitalRead(VALVESET_PIN) & 1) << 4;
    // Serial.print("fd");
    // Serial.println(isfdPressed);
    // Serial.print("fire");
    // Serial.println(isFireButtonPressed);

    CAN.sendData(CAN_ID_BUTTON_STATE, &data, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}