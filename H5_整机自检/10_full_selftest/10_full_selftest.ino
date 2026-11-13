/*
 * ============================================================
 *  H5 · 整机自检（7 个模块一次全测）
 * ------------------------------------------------------------
 *  接线（与之前各模块单测一致）：
 *    超声波   VCC→3V3   GND→GND   Trig→GPIO25   Echo→GPIO18
 *    蜂鸣器   VCC→3V3   GND→GND   I/O →GPIO4
 *    MPU6050  VCC→3V3   GND→GND   SDA→GPIO21    SCL →GPIO22
 *    按键     一脚→GPIO13   对角脚→GND
 *    SU-03T   VCC→3V3   GND→GND   TXD→GPIO16    RXD 不接
 *    SYN6288  VCC→VIN(5V) GND→GND RXD→GPIO17    TXD 不接   喇叭→SPK
 *    GPS      VCC→3V3   GND→GND   TXD→GPIO32    RXD 不接
 *
 *  串口监视器 115200，输出按【1/7】…【7/7】顺序，
 *  每项都会打印"通过/异常"，照着看就行。
 * ============================================================
 */

#include <Wire.h>

// ---------------------- 引脚定义 ----------------------
const int PIN_TRIG   = 25;   // 超声波 触发
const int PIN_ECHO   = 18;   // 超声波 回响
const int PIN_BUZZER = 4;    // 无源蜂鸣器
const int PIN_BUTTON = 13;   // SOS 按键
const int PIN_SDA    = 21;   // MPU6050 数据
const int PIN_SCL    = 22;   // MPU6050 时钟

// ------------------- SYN6288 的 GBK 文本 -------------------
// 之前已实测可用，直接复用，避免中文乱码
const byte TXT_OBST[] = { 0xC7, 0xB0, 0xB7, 0xBD, 0xD3, 0xD0, 0xD5, 0xCF, 0xB0, 0xAD }; // 前方有障碍
const byte TXT_OK[]   = { 0xB2, 0xE2, 0xCA, 0xD4, 0xB3, 0xC9, 0xB9, 0xA6 };             // 测试成功

// ============================================================
//  setup：只做一次，初始化所有外设
// ============================================================
void setup() {
  Serial.begin(115200);          // 调试串口
  delay(300);

  pinMode(PIN_TRIG, OUTPUT);     // 超声波触发脚 = 输出
  pinMode(PIN_ECHO, INPUT);      // 超声波回响脚 = 输入
  pinMode(PIN_BUZZER, OUTPUT);   // 蜂鸣器 = 输出
  pinMode(PIN_BUTTON, INPUT_PULLUP); // 按键用内部上拉，松开为高电平
  digitalWrite(PIN_TRIG, LOW);

  Wire.begin(PIN_SDA, PIN_SCL);  // 启动 I2C

  // GPS 用 Serial1，重映射到 32(收) / 33(发)
  Serial1.begin(9600, SERIAL_8N1, 32, 33);
  // SU-03T(收) 与 SYN6288(发) 共用 Serial2：RX=16，TX=17
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println();
  Serial.println("========== 整机自检开始 ==========");
  Serial.println("串口 115200；结果按 1/7 ~ 7/7 依次打印");
  Serial.println();
}

// ============================================================
//  loop：一轮测完，停 8 秒重来（方便反复观察）
// ============================================================
void loop() {
  Serial.println("----------------------------------------");

  testBuzzer();       // 1/7
  testUltrasonic();   // 2/7
  testButton();       // 3/7
  testMPU();          // 4/7
  testGPS();          // 5/7
  testSU03T();        // 6/7
  testSyn6288();      // 7/7

  Serial.println();
  Serial.println("===== 一轮结束，8 秒后重新开始 =====");
  Serial.println();
  delay(8000);
}

// ============================================================
// 1/7  蜂鸣器
// ============================================================
void testBuzzer() {
  Serial.println("【1/7】蜂鸣器：应先低音、再高音各响一声");
  tone(PIN_BUZZER, 800, 400);
  delay(700);
  tone(PIN_BUZZER, 2000, 400);
  delay(700);
  noTone(PIN_BUZZER);
  Serial.println("      听到两声 → 通过；没声音 → 检查 VCC/GND/I-O 三根线");
  Serial.println();
}

// ============================================================
// 2/7  超声波
// ============================================================
long readDistanceCM() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH, 30000); // 最多等 30ms
  if (duration == 0) return -1;                   // 没收到回波
  return duration / 58;                           // 每厘米约 58us
}

void testUltrasonic() {
  Serial.println("【2/7】超声波：连测 5 次，手放在探头前晃动");
  int valid = 0;
  for (int i = 0; i < 5; i++) {
    long cm = readDistanceCM();
    if (cm > 0 && cm < 400) {
      valid++;
      Serial.print("      第"); Serial.print(i + 1); Serial.print("次: ");
      Serial.print(cm); Serial.println(" cm");
    } else {
      Serial.print("      第"); Serial.print(i + 1); Serial.println("次: 无回波");
    }
    delay(120);
  }
  if (valid >= 3) Serial.println("      → 通过（能测到距离）");
  else            Serial.println("      → 异常（大多无回波，检查 Trig=25 / Echo=18 是否接反）");
  Serial.println();
}

// ============================================================
// 3/7  按键
// ============================================================
void testButton() {
  Serial.println("【3/7】按键：现在不要按");
  int v = digitalRead(PIN_BUTTON);
  Serial.print("      当前电平 = ");
  Serial.println(v == HIGH ? "HIGH（松开，正常）" : "LOW（被按下或接错）");
  Serial.println("      → 松开时读到 HIGH 就通过；一直 LOW 检查是否把 GND 接到了 GPIO13");
  Serial.println();
}

// ============================================================
// 4/7  MPU6050
// ============================================================
byte i2cRead(byte dev, byte reg) {
  Wire.beginTransmission(dev);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)dev, 1);
  if (Wire.available()) return Wire.read();
  return 0xFF;
}

void testMPU() {
  Serial.println("【4/7】MPU6050：检查 I2C 通信和原始数据");

  // 唤醒：往 0x6B 写 0x00（退出睡眠）
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0x00);
  byte err = Wire.endTransmission();
  if (err != 0) {
    Serial.print("      → 异常：找不到 0x68，I2C 错误码 ");
    Serial.println(err);
    Serial.println("        检查 SDA=21 / SCL=22 有没有接反、VCC 是否 3.3V");
    Serial.println();
    return;
  }

  byte who = i2cRead(0x68, 0x75);   // WHO_AM_I 固定值 0x68
  Serial.print("      WHO_AM_I = 0x");
  Serial.println(who, HEX);

  // 读 6 字节加速度：0x3B~0x40
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((int)0x68, 6);

  if (Wire.available() >= 6) {
    int16_t ax = (Wire.read() << 8); ax |= Wire.read();
    int16_t ay = (Wire.read() << 8); ay |= Wire.read();
    int16_t az = (Wire.read() << 8); az |= Wire.read();

    Serial.print("      加速度(g)：X="); Serial.print(ax / 16384.0, 2);
    Serial.print("  Y=");              Serial.print(ay / 16384.0, 2);
    Serial.print("  Z=");              Serial.println(az / 16384.0, 2);
    Serial.println("      → 三个数不为 0 且合矢量约 1g 就通过");
  } else {
    Serial.println("      → 异常：读不到加速度数据");
  }
  Serial.println();
}

// ============================================================
// 5/7  GPS
// ============================================================
void testGPS() {
  Serial.println("【5/7】GPS：读取 4 秒，统计收到的 NMEA 语句");
  int dollars = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < 4000) {
    while (Serial1.available()) {
      char c = Serial1.read();
      if (c == '$') dollars++;       // 每条 NMEA 语句都以 $ 开头
    }
  }
  Serial.print("      4 秒内收到语句数 = ");
  Serial.println(dollars);
  if (dollars > 0) Serial.println("      → 通过（模块在往外吐数据，室内收不到星属正常）");
  else             Serial.println("      → 异常（TXD=32 没接上，或没共地）");
  Serial.println();
}

// ============================================================
// 6/7  SU-03T
// ============================================================
void testSU03T() {
  Serial.println("【6/7】SU-03T：读取 3 秒，看串口有没有字节过来");
  int n = 0;
  unsigned long t0 = millis();
  while (millis() - t0 < 3000) {
    while (Serial2.available()) {
      byte b = Serial2.read();
      Serial.print("      收到: 0x");
      if (b < 16) Serial.print("0");
      Serial.println(b, HEX);
      n++;
    }
  }
  Serial.print("      3 秒共收到字节 = ");
  Serial.println(n);
  if (n > 0) Serial.println("      → 通过（连线和供电都通）");
  else       Serial.println("      → 暂无数据：出厂未配置词条时本来就不发数据，属正常；先看供电和 TXD=16");
  Serial.println();
}

// ============================================================
// 7/7  SYN6288
// ============================================================
void speak(const byte* text, int textLen) {
  byte frame[64];
  frame[0] = 0xFD;                     // 帧头
  int dataLen = textLen + 3;
  frame[1] = (dataLen >> 8) & 0xFF;
  frame[2] = dataLen & 0xFF;
  frame[3] = 0x01;                     // 命令：合成并播放
  frame[4] = 0x00;                     // 编码：GBK

  for (int i = 0; i < textLen; i++) frame[5 + i] = text[i];

  byte xorSum = 0;
  int n = 5 + textLen;
  for (int i = 0; i < n; i++) xorSum ^= frame[i];
  frame[n] = xorSum;

  Serial2.write(frame, n + 1);
}

void testSyn6288() {
  Serial.println("【7/7】SYN6288：喇叭应依次读出「前方有障碍」「测试成功」");
  speak(TXT_OBST, sizeof(TXT_OBST));
  delay(2500);
  speak(TXT_OK, sizeof(TXT_OK));
  delay(2000);
  Serial.println("      → 两句话都听清就通过；没声音检查 VCC 是否接 VIN(5V)、RXD=17");
  Serial.println();
}
