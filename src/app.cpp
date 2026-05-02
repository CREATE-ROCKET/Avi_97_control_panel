#include "app.h"

uint8_t buttonState = 0;
uint8_t mainState = 0;
uint8_t valveState = 0;
int16_t valveAngleX10 = 0;

unsigned long lastMainRx = 0;
unsigned long lastValveRx = 0;

SemaphoreHandle_t stateMutex = nullptr;
CAN_CREATE CAN(true);
