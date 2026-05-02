#include "tasks.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "../app.h"

void SampleButtonTask(void *pvParameters)
{
  uint8_t history[4] = {0};
  uint8_t idx = 0;

  while (1)
  {
    uint8_t currentRaw = 0;
    currentRaw |= (digitalRead(DUMP_PIN) == HIGH) ? (1 << 0) : 0;
    currentRaw |= (digitalRead(FIRE_PIN) == HIGH) ? (1 << 1) : 0;
    currentRaw |= (digitalRead(FILL_PIN) == HIGH) ? (1 << 2) : 0;
    currentRaw |= (digitalRead(SEPARATE_PIN) == HIGH) ? (1 << 3) : 0;
    currentRaw |= (digitalRead(VALVESET_PIN) == HIGH) ? (1 << 4) : 0;
    currentRaw |= (digitalRead(O2_PIN) == HIGH) ? (1 << 5) : 0;
    currentRaw |= (digitalRead(VALVE_OPEN_PIN) == HIGH) ? (1 << 6) : 0;
    // currentRaw |= (digitalRead(VALVE_OPEN_PIN) == HIGH) ? (1 << 7) : 0;

    history[idx] = currentRaw;
    idx = (idx + 1) % 4;

    uint8_t all_high = history[0] & history[1] & history[2] & history[3];
    uint8_t all_low = ~history[0] & ~history[1] & ~history[2] & ~history[3];

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    buttonState |= all_high;
    buttonState &= ~all_low;
    xSemaphoreGive(stateMutex);

    vTaskDelay(SAMPLING_RATE_MS / portTICK_PERIOD_MS);
  }
}
