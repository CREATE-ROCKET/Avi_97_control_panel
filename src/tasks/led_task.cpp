#include "tasks.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "../app.h"

void StateLEDTask(void *pvParameters)
{
  bool ledState = false;
  while (1)
  {
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    bool mainAlive = (millis() - lastMainRx < COMMUNICATION_TIMEOUT_MS);
    bool valveAlive = (millis() - lastValveRx < COMMUNICATION_TIMEOUT_MS);
    xSemaphoreGive(stateMutex);

    if (mainAlive && valveAlive)
    {
      digitalWrite(MCU_LUMP_PIN, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    else
    {
      if (!mainAlive)
      {
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        mainState = 4;
        xSemaphoreGive(stateMutex);
      }
      if (!valveAlive)
      {
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        valveState = 3;
        xSemaphoreGive(stateMutex);
      }

      ledState = !ledState;
      digitalWrite(MCU_LUMP_PIN, ledState ? HIGH : LOW);
      vTaskDelay(ERROR_COMMUNICATION_TIMEOUT_MS / portTICK_PERIOD_MS);
    }
  }
}
