#define DEBUG
#include <Arduino.h>
#include <PLG_datastring.h>
#include <hardwareSerial.h>
#include <ModbusMaster.h>
#include <LoRa.h>
#include <string.h>

ModbusMaster sensor_1;
ModbusMaster sensor_2;

// define the pins used by the transceiver module
#define ss 05            // Slave Select pin
#define rst 04           // Reset pin
#define dio0 14          // DIO0 pin
                         // mosi, miso, sck are defined in the SPI library
                         // for ESP32, the default pins are: mosi=23, miso=19, sck=18
#define led_connected 26 // LED pin for connection status
#define led_slave 33     // LED pin for master status
#define led_master 25    // LED pin for slave status
#define TX 17            // TX pin for RS485 module
#define RX 16            // RX pin for RS485 module
#define ADC_PHdat 34
String ID_control = "0";
String ID_CB = "1";
String ID_ = "2";
String ID_master = "3";
HardwareSerial mySerial(2); // Sử dụng UART2

#define ID_SENSOR_1 1 // cam bien anh sang
#define ID_SENSOR_2 2 // cam bien nhiet do |do am

float temp, hum, PH_dat;
unsigned long lastTime = 0;
uint8_t Com[8] = {0x01, 0x03, 0x00, 0x02, 0x00, 0x02, 0x65, 0xCB};
float LUX;

uint8_t readN(uint8_t *buf, size_t len)
{
  size_t offset = 0, left = len;
  int16_t Tineout = 500;
  uint8_t *buffer = buf;
  long curr = millis();
  while (left)
  {
    if (mySerial.available())
    {
      buffer[offset] = mySerial.read();
      offset++;
      left--;
    }
    if (millis() - curr > Tineout)
    {
      break;
    }
  }
  return offset;
}

unsigned int CRC16_2(unsigned char *buf, int len)
{
  unsigned int crc = 0xFFFF;
  for (int pos = 0; pos < len; pos++)
  {
    crc ^= (unsigned int)buf[pos];
    for (int i = 8; i != 0; i--)
    {
      if ((crc & 0x0001) != 0)
      {
        crc >>= 1;
        crc ^= 0xA001;
      }
      else
      {
        crc >>= 1;
      }
    }
  }

  crc = ((crc & 0x00ff) << 8) | ((crc & 0xff00) >> 8);
  return crc;
}
void readLux(void)
{
  uint8_t Data[10] = {0};
  uint8_t ch = 0;
  bool flag = 1;
  while (flag)
  {
    delay(100);
    mySerial.write(Com, 8);
    delay(10);
    if (readN(&ch, 1) == 1)
    {
      if (ch == 0x01)
      {
        Data[0] = ch;
        if (readN(&ch, 1) == 1)
        {
          if (ch == 0x03)
          {
            Data[1] = ch;
            if (readN(&ch, 1) == 1)
            {
              if (ch == 0x04)
              {
                Data[2] = ch;
                if (readN(&Data[3], 6) == 6)
                {
                  if (CRC16_2(Data, 7) == (Data[7] * 256 + Data[8]))
                  {
                    LUX = (Data[3] * 0x1000000 + Data[4] * 0x10000 + Data[5] * 0x100 + Data[6]) / 1000.000;
                    flag = 0;
                  }
                }
              }
            }
          }
        }
      }
    }
    mySerial.flush();
  }
}

void read_tem_hud()
{
  uint8_t result = sensor_2.readHoldingRegisters(0x000, 2);
  if (result == sensor_2.ku8MBSuccess)
  {
    hum = sensor_2.getResponseBuffer(0) / 10.0;
    temp = sensor_2.getResponseBuffer(1) / 10.0;
  }
  // uint8_t luxx = sensor_1.readHoldingRegisters(0x03, 2);
  // if (luxx == sensor_1.ku8MBSuccess)
  // {
  //   LUX = sensor_1.getResponseBuffer(0) / 100.0; // Giả sử cảm biến trả về giá trị ánh sáng chia cho 100
  // }

  // Serial.print("Lux = ");
  // Serial.print(LUX, 3);
  // Serial.print(" (lux)");
}
void sen_lora_data()
{
  // Example of sending sensor data
  LoRa.beginPacket(); // Start a new packet
  LoRa.print(messages_sensor);
  LoRa.endPacket();               // Finish the packet and send it
  digitalWrite(led_master, HIGH); // Turn off LED for slave status
  delay(20);                      // Delay to ensure the message is sent
  digitalWrite(led_master, LOW);  // Turn off LED for slave status
}
void sen_lora_data_4()
{
  // Example of sending sensor data
  LoRa.beginPacket(); // Start a new packet
  LoRa.print(messages4);
  LoRa.endPacket();               // Finish the packet and send it
  digitalWrite(led_master, HIGH); // Turn off LED for slave status
  delay(20);                      // Delay to ensure the message is sent
  digitalWrite(led_master, LOW);  // Turn off LED for slave status
}
void sen_data_cambien()
{

  PLG_write_board_sensor(ID_CB, "slave1", "all_CB", String(temp, 2), String(hum, 2), String(LUX, 2), String(PH_dat, 2));
  sen_lora_data();
  PLG_write_4(ID_CB, ID_master, "sensor", "end");
  sen_lora_data_4();
  DEBUG_PRINTF("temp: %.2f   hum: %.2f   lux: %.2f   PH-dat: %.2f\n", temp, hum, LUX, PH_dat);
  // DEBUG_PRINTF("temp: %.2f   hum: %.2f   lux: %.2f   \n", temp, hum,LUX);
}
void setup()
{
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX, TX); // RX, TX là chân nhận và truyền dữ liệu
  sensor_1.begin(ID_SENSOR_1, Serial2);    // Cảm biến địa chỉ 1
  sensor_2.begin(ID_SENSOR_2, Serial2);    // Cảm biến địa chỉ 2
  mySerial.begin(9600);
  LoRa.setPins(ss, rst, dio0);
  pinMode(ADC_PHdat, INPUT);
  pinMode(led_slave, OUTPUT);       // Set LED pin as output
  pinMode(led_master, OUTPUT);      // Set LED pin for master status
  pinMode(led_connected, OUTPUT);   // Set LED pin for connection status
  digitalWrite(led_connected, LOW); // Turn off LED for slave status
  digitalWrite(led_slave, HIGH);    // Turn on LED for slave status
  digitalWrite(led_master, LOW);    // Turn off LED for master status
  // 433E6 for Asia
  while (!LoRa.begin(433E6))
  {
    Serial.print(".");
    delay(500);
  }
  DEBUG_PRINTLN("PLG_RUN_SENSOR");
}
void read_PHdat()
{
  PH_dat = analogRead(ADC_PHdat); // Đọc giá trị từ chân ADC_PHdat
  PH_dat = 10.0 - ((PH_dat / 4095.0) * 12.0);
}
void loop()
{

  unsigned long currentTime = millis();

  if (currentTime - lastTime >= 1500)
  {
    read_tem_hud();
    readLux();
    read_PHdat();
    sen_data_cambien();
    lastTime = currentTime;
  }
}
