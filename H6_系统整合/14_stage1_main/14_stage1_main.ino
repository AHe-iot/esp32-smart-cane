/*
 * ============================================================
 *  H6 · 阶段 1 收官：主程序（避障 + SOS + 跌倒 三合一）
 * ------------------------------------------------------------
 *  接线（全部沿用锁定口径）：
 *    超声波  Trig→25  Echo→18  VCC→3V3
 *    蜂鸣器  I/O→4    VCC→3V3
 *    按键    GPIO13（短按发 SOS；跌倒报警时按一下解除）
 *    MPU6050 SDA→21   SCL→22  VCC→3V3
 *
 *  蜂鸣器优先级（高 → 低）：
 *    跌倒报警  >  SOS 摩斯码  >  避障节奏
 *
 *  全程 millis() 非阻塞；串口 115200。
 * ============================================================
 */

#include <Wire.h>

// ---------------------- 引脚 ----------------------
const int PIN_TRIG = 25, PIN_ECHO = 18;
const int PIN_BUZZ = 4, PIN_BUTTON = 13;
const int PIN_SDA = 21, PIN_SCL = 22;

// ---------------------- 避障 ----------------------
const int MEASURE_GAP = 60;
unsigned long lastMeasure = 0;
long samples[3] = { -1, -1, -1 };
int sampleIdx = 0;
long distCM = -1;
int obsLevel = 0;

// ---------------------- 跌倒 ----------------------
const float IMPACT_G = 2.5;
const float TILT_DEG = 45.0;
const int CHECK_MS = 1200;
enum FallState { F_NORMAL, F_CHECK, F_FALL };
int fallState = F_NORMAL;
unsigned long fallStart = 0;
float gx, gy, gz, mag, angle;
unsigned long lastRead = 0;

// ---------------------- 按键 ----------------------
int lastRaw = HIGH, stableState = HIGH;
unsigned long lastBounce = 0;
const int DEBOUNCE_MS = 30;
bool pressConsumed = false;

// ---------------------- SOS 摩斯码 ----------------------
const int DOT = 150, DASH = 450;
const int elemDur[9] = { DOT, DOT, DOT, DASH, DASH, DASH, DOT, DOT, DOT };
enum SosPhase { S_IDLE, S_ON, S_GAP };
int sosPhase = S_IDLE;
int elem = 0;
unsigned long sosPhaseStart = 0;

// ---------------------- 通用蜂鸣调度 ----------------------
bool buzzing = false;
unsigned long beepStart = 0, lastBeep = 0;
unsigned long lastPrint = 0;

// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  digitalWrite(PIN_TRIG, LOW);
  noTone(PIN_BUZZ);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  Serial.println();
  Serial.println("===== 阶段1 主程序：避障 + SOS + 跌倒 =====");
}

// ============================================================
void loop() {
  unsigned long now = millis();

  // ========== 1. 按键消抖 ==========
  int raw = digitalRead(PIN_BUTTON);
  if (raw != lastRaw) { lastBounce = now; lastRaw = raw; }
  if (now - lastBounce >= DEBOUNCE_MS && raw != stableState) stableState = raw;

  // 一次"按下"只处理一次
  if (stableState == LOW && !pressConsumed) {
    pressConsumed = true;
    if (fallState == F_FALL) {
      fallState = F_NORMAL;
      noTone(PIN_BUZZ);
      buzzing = false;
      Serial.println("<<< 手动解除跌倒报警");
    } else if (sosPhase == S_IDLE) {
      sosPhase = S_ON;
      elem = 0;
      sosPhaseStart = now;
      noTone(PIN_BUZZ);
      buzzing = false;
      tone(PIN_BUZZ, 1200);
      Serial.println(">>> SOS 触发");
    }
  }
  if (stableState == HIGH) pressConsumed = false;

  // ========== 2. 超声波 + 避障档位 ==========
  if (now - lastMeasure >= MEASURE_GAP) {
    lastMeasure = now;
    long cm = readCM();
    if (cm > 0) {
      samples[sampleIdx] = cm;
      sampleIdx = (sampleIdx + 1) % 3;
      distCM = median3(samples[0], samples[1], samples[2]);
    }
  }
  if (distCM < 0 || distCM > 100) obsLevel = 0;
  else if (distCM > 50) obsLevel = 1;
  else if (distCM > 20) obsLevel = 2;
  else if (distCM > 5)  obsLevel = 3;
  else                  obsLevel = 4;

  // ========== 3. 跌倒检测 ==========
  if (now - lastRead >= 25) {
    lastRead = now;
    readAccel();
    mag = sqrt(gx * gx + gy * gy + gz * gz);
    angle = atan2(sqrt(gx * gx + gy * gy), gz) * 180.0 / PI;
  }

  if (fallState == F_NORMAL && mag > IMPACT_G) {
    fallState = F_CHECK;
    fallStart = now;
    Serial.println(">>> 检测到冲击，观察姿态");
  } else if (fallState == F_CHECK && now - fallStart >= CHECK_MS) {
    if (angle > TILT_DEG) {
      fallState = F_FALL;
      noTone(PIN_BUZZ);
      buzzing = false;
      Serial.println("!!! 确认跌倒，报警");
    } else {
      fallState = F_NORMAL;
      Serial.println("    误报解除");
    }
  }

  // ========== 4. 蜂鸣器（按优先级） ==========
  if (fallState == F_FALL) {
    scheduleBeep(150, 110, 1500);                       // 跌倒：急促
  } else if (sosPhase != S_IDLE) {
    if (sosPhase == S_ON) {
      if (now - sosPhaseStart >= elemDur[elem]) {
        noTone(PIN_BUZZ);
        sosPhase = S_GAP;
        sosPhaseStart = now;
      }
    } else {
      int gap = (elem == 2 || elem == 5) ? 300 : 150;
      if (now - sosPhaseStart >= gap) {
        elem++;
        if (elem >= 9) {
          sosPhase = S_IDLE;
          noTone(PIN_BUZZ);
        } else {
          sosPhase = S_ON;
          sosPhaseStart = now;
          tone(PIN_BUZZ, 1200);
        }
      }
    }
  } else if (obsLevel > 0) {
    int intervals[4] = { 900, 550, 280, 150 };
    scheduleBeep(intervals[obsLevel - 1], 120, 1000);   // 避障节奏
  } else {
    noTone(PIN_BUZZ);
    buzzing = false;
  }

  // ========== 5. 综合打印 ==========
  if (now - lastPrint >= 500) {
    lastPrint = now;
    Serial.print("距离: ");
    if (distCM < 0) Serial.print("--");
    else            Serial.print(distCM);
    Serial.print("cm 避障档: "); Serial.print(obsLevel);
    Serial.print(" | 倾角: ");  Serial.print(angle, 1);
    Serial.print("° | 跌倒: ");
    if (fallState == F_NORMAL) Serial.print("正常");
    else if (fallState == F_CHECK) Serial.print("观察");
    else Serial.print("报警");
    if (sosPhase != S_IDLE) Serial.print(" | SOS播放中");
    Serial.println();
  }
}

// ============================================================
// 通用蜂鸣：interval 两声间隔，onMs 每声时长，freq 频率
// ============================================================
void scheduleBeep(int interval, int onMs, int freq) {
  unsigned long now = millis();
  if (!buzzing && now - lastBeep >= interval) {
    tone(PIN_BUZZ, freq);
    buzzing = true;
    beepStart = now;
    lastBeep = now;
  }
  if (buzzing && now - beepStart >= onMs) {
    noTone(PIN_BUZZ);
    buzzing = false;
  }
}

// ---------------------- 超声波 ----------------------
long readCM() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long d = pulseIn(PIN_ECHO, HIGH, 30000);
  if (d == 0) return -1;
  return d / 58;
}

long median3(long a, long b, long c) {
  if (a > b) { long t = a; a = b; b = t; }
  if (b > c) { long t = b; b = c; c = t; }
  if (a > b) { long t = a; a = b; b = t; }
  return b;
}

// ---------------------- 加速度 ----------------------
void readAccel() {
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((int)0x68, 6);
  if (Wire.available() >= 6) {
    int16_t ax = Wire.read() << 8; ax |= Wire.read();
    int16_t ay = Wire.read() << 8; ay |= Wire.read();
    int16_t az = Wire.read() << 8; az |= Wire.read();
    gx = ax / 16384.0;
    gy = ay / 16384.0;
    gz = az / 16384.0;
  }
}
