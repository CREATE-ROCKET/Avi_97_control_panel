#include <Arduino.h>
#include "CANCREATE.h"
#define Vread() 
/*GPIOの値の設定*/
#define serial1RX 21 /*シリアル変換モジュール*/
#define serial1TX 18
#define FILL 16
#define VALVESET 4
#define DUMP 34
#define FIRE 35
#define NOD 17
#define MCU_LUMP 15
#define CAN_TX 32
#define CAN_RX 33
#define NICHROME_SIGNAL 0x10a

constexpr uint8_t readpins[5] = {GPIO_NUM_16, GPIO_NUM_4, GPIO_NUM_34, GPIO_NUM_35, GPIO_NUM_17};
unsigned long lastTime = 0;
short led_blink_count = 0;
/*以下はデバウンス用*/
char iffirepushingwithdebouce = 0;
char ifNODpushingwithdebouce = 0;
unsigned long firedebouncetime = 0;
unsigned long NODdebouncetime = 0;
bool firecheck = false;
bool NODcheck = false;
CAN_CREATE CAN(true);
void setup()
{
  pinMode(MCU_LUMP, OUTPUT);
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, serial1RX, serial1TX);
  // tx,rx  32,33:コンパネ　　13,27:中継
  Serial.println("CAN Sender");
  // 100 kbpsでCANを動作させる
  if (CAN.begin(100E3, CAN_RX, CAN_TX))
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
  if (led_blink_count == 1000)
  {
    led_blink_count = 0;
  }
}

void send()
{ // send the switch data to the CAN bus in id 0x101(to plc)
  uint8_t data = 0;
  data |= (digitalRead(DUMP) & 1) << 0;
  data |= (digitalRead(FILL) & 1) << 1;
  data |= ((iffirepushingwithdebouce & (!digitalRead(NOD))) & 1) << 2;
  data |= (ifNODpushingwithdebouce) << 3;
  data |= (digitalRead(VALVESET) & 1) << 4;
  if (CAN.sendData(0x101, &data, 1))
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
  if (digitalRead(NOD) == HIGH)
  {
    if (!NODcheck)
    {
      NODdebouncetime = millis();
      NODcheck = true;
    }
    if (NODdebouncetime != 0 && millis() - NODdebouncetime > 20)
    {
      ifNODpushingwithdebouce = 1;
    }
  }
  else
  {
    ifNODpushingwithdebouce = 0;
    NODcheck = false;
  }
}

void loop()
{
  checkPLCisDEAD();
  send();
  updatepins();
  if (CAN.available())
  {
    can_return_t message;
    while (!CAN.readWithDetail(&message))
    {
      switch (message.id)
      {
      case 0x401: /*from main_valve*/
        Serial1.print("innner angle is");
        Serial1.println(message.data[0] - 128, DEC);
        send();
        break;
      case 0x402: /*from fd_valve*/
        Serial1.print("FD angle is");
        Serial1.println(message.data[0] - 128, DEC);
        send();
        break;
      case 0x403: /*from plc*/
        lastTime = millis();
        //   for (int i = 0; i < message.size; i++)
        //   {
        //     Serial1.print(message.data[i], BIN);
        //     Serial1.print(" ");
        //   }
        break;
      default:
        break;
      }
      delay(1);
    }
  }
  delay(10);
}