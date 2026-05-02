#include "tasks.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "../app.h"

namespace
{
int16_t readInt16Le(const char *data)
{
  uint16_t raw = static_cast<uint8_t>(data[0]) |
                 (static_cast<uint16_t>(static_cast<uint8_t>(data[1])) << 8);
  return static_cast<int16_t>(raw);
}
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
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        switch (message.id)
        {
        case CAN_ID_MAIN_STATE:
          if (message.size > 0)
          {
            lastMainRx = millis();
            mainState = message.data[0];
          }
          break;
        case CAN_ID_MAIN_VALVE_STATE:
          if (message.size > 0)
          {
            lastValveRx = millis();
            valveState = message.data[0];
          }
          break;
        case CAN_ID_MAIN_VALVE_ANGLE:
          if (message.size >= ANGLE_CAN_SIZE)
          {
            lastValveRx = millis();
            valveAngleX10 = readInt16Le(message.data);
          }
          break;
        default:
          break;
        }
        xSemaphoreGive(stateMutex);
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void CANSendTask(void *pvParameters)
{
  while (1)
  {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    uint8_t bState = buttonState;
    xSemaphoreGive(stateMutex);

    uint8_t fire = (bState >> 1) & 1;
    uint8_t fill = (bState >> 2) & 1;
    uint8_t separate = (bState >> 3) & 1;
    uint8_t o2_test = (bState >> 5) & 1;

    uint8_t data = 0;
    data |= bState & 1;               // bit0: dump
    data |= (fire && !fill) << 1;     // bit1: fire & !fill
    data |= fill << 2;                // bit2: fill
    data |= (separate && !fill) << 3; // bit3: separate & !fill
    data |= bState & (1 << 4);        // bit4: valve_set
    data |= (!fire && o2_test) << 5;  // bit5: !fire & o2_test
    data |= bState & (1 << 6);        // bit6: open_valve

    CAN.sendData(CAN_ID_BUTTON_STATE, &data, 1);
    // vTaskDelay(5 / portTICK_PERIOD_MS);
    // CAN.sendData(0x200, &data, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}
