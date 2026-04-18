#include <Arduino.h>
#include "CANCREATE.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// --- Pin Definitions ---
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

// --- CAN ID Definitions ---
constexpr uint32_t CAN_ID_BUTTON_STATE = 0x101;
constexpr uint32_t CAN_ID_MAIN_VALVE_ANGLE = 0x102;
constexpr uint32_t CAN_ID_MAIN_STATE = 0x103;
constexpr uint32_t CAN_ID_MAIN_VALVE_STATE = 0x107;

// --- Constants ---
constexpr long COMMUNICATION_TIMEOUT_MS = 3000;
constexpr long ERROR_COMMUNICATION_TIMEOUT_MS = 300;
constexpr int SAMPLING_RATE_MS = 8; // デバウンス用サンプリングレート
constexpr int16_t OPEN_ANGLE = -35; //
constexpr int16_t CLOSE_ANGLE = 55; //

// --- Global State Variables & Mutexes ---
uint8_t buttonState = 0;
uint8_t mainState = 0;
uint8_t valveState = 0;
uint8_t valveAngle = 0;

unsigned long lastMainRx = 0;
unsigned long lastValveRx = 0;

SemaphoreHandle_t stateMutex;

CAN_CREATE CAN(true);
// SPIClass hspi(HSPI);
// Adafruit_ILI9341 tft = Adafruit_ILI9341(&hspi, TFT_DC, TFT_CS, TFT_RST);

// --- Task & Function Prototypes ---
void CANRecvTask(void *pvParameters);
void CANSendTask(void *pvParameters);
void SampleButtonTask(void *pvParameters);
void StateLEDTask(void *pvParameters);
void DisplayTask(void *pvParameters);

void setup()
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

  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, SERIAL1_RX_PIN, SERIAL1_TX_PIN);

  stateMutex = xSemaphoreCreateMutex();

  // // --- TFTディスプレイの初期化 ---
  // hspi.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  // tft.begin();
  // tft.setRotation(1); // 1 = 横向き表示
  // tft.fillScreen(ILI9341_BLACK);
  // tft.setTextSize(2); // 文字サイズ（適宜調整してください）
  // delay(1000);

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
  // xTaskCreateUniversal(DisplayTask, "DisplayTask", 4096, NULL, 1, NULL, PRO_CPU_NUM);
}

const char *get_valve_state_str(uint8_t state)
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

const char *get_main_state_str(uint8_t state)
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

void loop()
{
  static bool burned = false; // 点火フラグを保持

  // グローバル変数の安全な読み出し
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  uint8_t v_com_state = valveState;
  uint8_t v_angle_raw = valveAngle;
  uint8_t m_state = mainState;
  uint8_t b_state = buttonState;
  xSemaphoreGive(stateMutex);

  // 点火状態の判定
  if (m_state == 1)
  {
    burned = true; //
  }

  // 角度計算とステータス判定
  int16_t angle = (int16_t)v_angle_raw - 135; //
  const char *angle_status = "Invalid";
  if (angle > (CLOSE_ANGLE - 20) && angle < (CLOSE_ANGLE + 20))
  {
    angle_status = "Close!"; //
  }
  else if (angle > (OPEN_ANGLE - 20) && angle < (OPEN_ANGLE + 20))
  {
    angle_status = "Open!"; //
  }

  // PC(Serial1)への送信
  Serial1.printf(
      "burned: %s, valve_com_state: %s, main_state: %s, MainAngle: %s (%d) \r\n",
      burned ? "Done" : "Yet",
      get_valve_state_str(v_com_state),
      get_main_state_str(m_state),
      angle_status,
      angle);
  Serial1.printf("DUMP:%d  FIRE:%d\r\n", (b_state >> 0) & 1, (b_state >> 1) & 1);
  Serial1.printf("FILL:%d  SEP :%d\r\n", (b_state >> 2) & 1, (b_state >> 3) & 1);
  Serial1.printf("SET :%d  O2  :%d\r\n", (b_state >> 4) & 1, (b_state >> 5) & 1);
  Serial1.printf("VLV_OPN :%d\r\n", (b_state >> 6) & 1);

  // 1秒ごとに送信
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

// 状態表示LEDタスク（Rustの mainループ 相当）
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
      // 正常時：常時点灯
      digitalWrite(MCU_LUMP_PIN, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    else
    {
      // エラー時：一定周期(300ms)で点滅
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

// ボタンのサンプリングタスク
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

    // 4回連続High / Lowの判定
    uint8_t all_high = history[0] & history[1] & history[2] & history[3];
    uint8_t all_low = ~history[0] & ~history[1] & ~history[2] & ~history[3];

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    // 確実にHigh/Lowになったビットのみを更新し、それ以外は維持
    buttonState |= all_high;
    buttonState &= ~all_low;
    xSemaphoreGive(stateMutex);

    vTaskDelay(SAMPLING_RATE_MS / portTICK_PERIOD_MS);
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
        case CAN_ID_MAIN_STATE: // 0x103
          if (message.size > 0)
          {
            lastMainRx = millis();
            mainState = message.data[0];
          }
          break;
        case CAN_ID_MAIN_VALVE_STATE: // 0x107
          if (message.size > 0)
          {
            lastValveRx = millis();
            valveState = message.data[0];
          }
          break;
        case CAN_ID_MAIN_VALVE_ANGLE: // 0x102
          if (message.size > 0)
          {
            lastValveRx = millis();
            valveAngle = message.data[0];
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

// void DisplayTask(void *pvParameters)
// {
//   static bool burned = false;

//   while (1)
//   {
//     // グローバル変数の安全な読み出し
//     xSemaphoreTake(stateMutex, portMAX_DELAY);
//     uint8_t v_com_state = valveState;
//     uint8_t v_angle_raw = valveAngle;
//     uint8_t m_state = mainState;
//     uint8_t b_state = buttonState;
//     xSemaphoreGive(stateMutex);

//     if (m_state == 1)
//       burned = true;

//     int16_t angle = (int16_t)v_angle_raw - 135;
//     const char *angle_status = "Inv";
//     if (angle > (CLOSE_ANGLE - 20) && angle < (CLOSE_ANGLE + 20))
//       angle_status = "Cls";
//     else if (angle > (OPEN_ANGLE - 20) && angle < (OPEN_ANGLE + 20))
//       angle_status = "Opn";

//     // --- 描画開始 ---
//     tft.setCursor(0, 0);
//     tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);

//     // 基板ステータスセクション
//     tft.setTextSize(2);
//     tft.printf("System Status\n");
//     tft.printf("Burned: %-10s\n", burned ? "DONE" : "YET");
//     tft.printf("Valve : %-10s\n", get_valve_state_str(v_com_state));
//     tft.printf("Main  : %-10s\n", get_main_state_str(m_state));
//     tft.printf("Angle : %s (%4d)\n", angle_status, angle);

//     tft.println("\n--- Button States ---");

//     tft.printf("DUMP:%d  FIRE:%d\n", (b_state >> 0) & 1, (b_state >> 1) & 1);
//     tft.printf("FILL:%d  SEP :%d\n", (b_state >> 2) & 1, (b_state >> 3) & 1);
//     tft.printf("SET :%d  O2  :%d\n", (b_state >> 4) & 1, (b_state >> 5) & 1);
//     tft.printf("RST :%d\n", (b_state >> 6) & 1);

//     // // 画面下部にデバッグ用に生データを右寄せ表示（好みに合わせて）
//     // tft.setCursor(180, 220);
//     // tft.setTextSize(1);
//     // tft.printf("RAW:%02X", b_state);

//     vTaskDelay(500 / portTICK_PERIOD_MS);
//   }
// }