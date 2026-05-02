#include "reporting.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "app.h"

namespace
{
const char *getValveStateStr(uint8_t state)
{
  switch (state)
  {
  case 0:
    return "Normal";
  case 1:
    return "Communication ERROR";
  case 2:
    return "VALVE CAN ERROR";
  case 3:
    return "VALVE-CONTROL CAN ERROR";
  default:
    return "Unreachable";
  }
}

const char *getMainStateStr(uint8_t state)
{
  switch (state)
  {
  case 0:
    return "Normal";
  case 1:
    return "IGNITION";
  case 2:
    return "TIMEOUT";
  case 3:
    return "MAIN CAN ERROR";
  case 4:
    return "MAIN-CONTROL CAN ERROR";
  default:
    return "Unreachable";
  }
}

const char *getAngleStatus(int16_t angleX10)
{
  const int16_t openAngleX10 = OPEN_ANGLE * ANGLE_SCALE;
  const int16_t closeAngleX10 = CLOSE_ANGLE * ANGLE_SCALE;
  const int16_t toleranceX10 = ANGLE_STATUS_TOLERANCE_DEG * ANGLE_SCALE;

  if (angleX10 > (closeAngleX10 - toleranceX10) && angleX10 < (closeAngleX10 + toleranceX10))
  {
    return "Close!";
  }
  if (angleX10 > (openAngleX10 - toleranceX10) && angleX10 < (openAngleX10 + toleranceX10))
  {
    return "Open!";
  }
  return "Invalid";
}
}

void sendStatusToPc()
{
  static bool burned = false;

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  uint8_t v_com_state = valveState;
  int16_t v_angle_x10 = valveAngleX10;
  uint8_t m_state = mainState;
  uint8_t b_state = buttonState;
  xSemaphoreGive(stateMutex);

  if (m_state == 1)
  {
    burned = true;
  }

  const char *angle_status = getAngleStatus(v_angle_x10);
  int32_t angle_abs_x10 = v_angle_x10 < 0 ? -static_cast<int32_t>(v_angle_x10) : static_cast<int32_t>(v_angle_x10);

  Serial1.printf(
      "burned: %s, valve_com_state: %s, main_state: %s, MainAngle: %s (%s%d.%d) \r\n",
      burned ? "Done" : "Yet",
      getValveStateStr(v_com_state),
      getMainStateStr(m_state),
      angle_status,
      v_angle_x10 < 0 ? "-" : "",
      static_cast<int>(angle_abs_x10 / ANGLE_SCALE),
      static_cast<int>(angle_abs_x10 % ANGLE_SCALE));
  Serial1.printf("DUMP:%d  FIRE:%d\r\n", (b_state >> 0) & 1, (b_state >> 1) & 1);
  Serial1.printf("FILL:%d  SEP :%d\r\n", (b_state >> 2) & 1, (b_state >> 3) & 1);
  Serial1.printf("SET :%d  O2  :%d\r\n", (b_state >> 4) & 1, (b_state >> 5) & 1);
  Serial1.printf("VLV_OPN :%d\r\n", (b_state >> 6) & 1);
}
