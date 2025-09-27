#include <Arduino.h>
#include "CANCREATE.h"
/*GPIOの値の設定*/
#define serial1RX 21
#define serial1TX 18
#define FILL 16
#define VALVESET 4
#define DUMP 34
#define FIRE 35
#define FD 17
#define MCU_LUMP 15
#define CAN_TX 32
#define CAN_RX 33

constexpr int readpins[5] = {GPIO_NUM_16, GPIO_NUM_4, GPIO_NUM_34, GPIO_NUM_35, GPIO_NUM_17};
bool PLC_DEAD = true;
unsigned long lastTime = 0;
int led_blink_count = 0;
/*以下はデバウンス用*/
int iffirepushingwithdebouce = 0;
int ifFDpushingwithdebouce = 0;
unsigned long firedebouncetime = 0;
unsigned long FDdebouncetime = 0;
bool firecheck = false;
bool FDcheck = false;
CAN_CREATE CAN(true);
void setup()
{
  pinMode(MCU_LUMP, OUTPUT);
  // Initialize configuration structures using macro initializers
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, serial1RX, serial1TX);
  // tx,rx  32,33:コンパネ　　13,27:中継
  Serial.println("CAN Sender");
  // 100 kbpsでCANを動作させる
  if (CAN.begin(100E3, CAN_RX, CAN_TX, 10))
  {
    Serial.println("Starting CAN failed!");
    while (1)
      ;
  }
  for (int i = 0; i < 5; i++)
  {
    pinMode(readpins[i], INPUT);
  }
}

void checkPLCisDEAD()
{
  // Serial.println(millis() - lastTime);
  if (millis() - lastTime < 3000)
  {
    digitalWrite(MCU_LUMP, HIGH);
  }
  else
  {
    PLC_DEAD = true;
    if (led_blink_count % 2 == 0)
    {
      digitalWrite(MCU_LUMP, LOW);
    }
    else
    {
      digitalWrite(MCU_LUMP, HIGH);
    }
    ++led_blink_count;
  }
}

void send()
{ // send the switch data to the CAN bus in id 0x101
  uint8_t message[4];
  for (int i = 0; i < 4; i++)
  {
    message[i] = 0;
  }
  // for (int j = 0; j < 5; j++)
  // {
  //   message.data[0]|=(digitalRead(readpins[j])&1)<<j;
  // }
  uint8_t data = 0;
  data |= (digitalRead(DUMP) & 1) << 0;
  data |= (digitalRead(FILL) & 1) << 1;
  data |= ((iffirepushingwithdebouce & (!digitalRead(FD))) & 1) << 2;
  data |= (ifFDpushingwithdebouce) << 3;
  data |= (digitalRead(VALVESET) & 1) << 4;
  message[0] = data;

  Serial.println(message[0], BIN);
  // Queue message for transmission
  if (CAN.sendData(0x101, message, 4))
  {
    Serial.println("failed to send CAN data");
  }
}
/*update the pins for the fire and FD buttons with debounce , witch is used to prevent the button from being pressed multiple times in a short time
,but it is not used for the other buttons,so it is not used for the FILL and DUMP buttons. However, it is used for the VALVESET button.
So, I will use the debounce for the fire and FD buttons. After all, I will use the debounce for the fire and FD buttons.*/
void updatepins()
{
  if (digitalRead(FIRE) == HIGH)
  {
    if (!firecheck)
    {
      firedebouncetime = millis();
      firecheck = true;
    }
    if (firedebouncetime != 0 && millis() - firedebouncetime > 20)
    {
      iffirepushingwithdebouce = 1;
    }
  }
  else
  {
    iffirepushingwithdebouce = 0;
    firecheck = false;
  }
  if (digitalRead(FD) == HIGH)
  {
    if (!FDcheck)
    {
      FDdebouncetime = millis();
      FDcheck = true;
    }
    if (FDdebouncetime != 0 && millis() - FDdebouncetime > 20)
    {
      ifFDpushingwithdebouce = 1;
    }
  }
  else
  {
    ifFDpushingwithdebouce = 0;
    FDcheck = false;
  }
}

void loop()
{
  checkPLCisDEAD();
  send();
  updatepins();
  if (Serial.available())
  {
    uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    char c = Serial.read();
    switch (c)
    {
    case 1: /*テスト*/
      if (CAN.sendData(0x7ff, data, 8))
      {
        Serial.println("failed to send CAN data");
      }
      break;
    case 2: /* サーボ角度リクエストテスト*/
      if (CAN.sendData(0x201, data, 8))
      {
        Serial.println("failed to send CAN data");
      }
      break;
    case 3: /* サーボ角度リクエストテスト*/
      if (CAN.sendData(0x301, data, 8))
      {
        Serial.println("failed to send CAN data");
      }
      break;
    default:
      break;
    }
  }
  if (CAN.available())
  {
    can_return_t message;
    while (!CAN.readWithDetail(&message))
    {
      if (message.id == 0x402)
      {
        Serial1.print("FD angle is");
        Serial1.println(message.data[0] - 128, DEC);
        send();
      }
      if (message.id == 0x401)
      {
        Serial1.print("innner angle is");
        Serial1.println(message.data[0] - 128, DEC);
        send();
      }
      if (message.id == 0x403)
      {
        // Serial1.print("plc is alive!");
        PLC_DEAD = false;
        lastTime = millis();
        //   for (int i = 0; i < message.size; i++)
        //   {
        //     Serial1.print(message.data[i], BIN);
        //     Serial1.print(" ");
        //   }
      }
      delay(1);
    }
  }
  delay(10);
}