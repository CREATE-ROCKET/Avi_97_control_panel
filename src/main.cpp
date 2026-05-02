#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "app.h"
#include "reporting.h"
#include "tasks/tasks.h"

void setupPins()
{
  pinMode(MCU_LUMP_PIN, OUTPUT);
  pinMode(FILL_PIN, INPUT_PULLDOWN);
  pinMode(VALVESET_PIN, INPUT_PULLDOWN);
  pinMode(DUMP_PIN, INPUT_PULLDOWN);
  pinMode(FIRE_PIN, INPUT_PULLDOWN);
  pinMode(SEPARATE_PIN, INPUT_PULLDOWN);
  pinMode(O2_PIN, INPUT);
  // pinMode(MAIN_RESET_PIN, INPUT_PULLDOWN);
  pinMode(VALVE_OPEN_PIN, INPUT_PULLDOWN);
}

void setupSerialPorts()
{
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, SERIAL1_RX_PIN, SERIAL1_TX_PIN);
}

void setup()
{
  setupPins();
  setupSerialPorts();

  stateMutex = xSemaphoreCreateMutex();

  Serial.println("CAN Control Panel");
  if (CAN.begin(100E3, CAN_RX_PIN, CAN_TX_PIN))
  {
    while (1)
      ;
  }
  delay(1000);

  xTaskCreateUniversal(CANRecvTask, "CANRecvTask", 2048, NULL, 1, NULL, APP_CPU_NUM);
  xTaskCreateUniversal(CANSendTask, "CANSendTask", 2048, NULL, 1, NULL, APP_CPU_NUM);
  xTaskCreateUniversal(SampleButtonTask, "SampleButtonTask", 2048, NULL, 1, NULL, APP_CPU_NUM);
  xTaskCreateUniversal(StateLEDTask, "StateLEDTask", 1024, NULL, 1, NULL, PRO_CPU_NUM);
}

void loop()
{
  sendStatusToPc();
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
