/*
 * ============================================================
 *  H6 · 系统整合 1/3：避障分级（超声波 + 蜂鸣器）
 * ------------------------------------------------------------
 *  接线（与整机自检一致）：
 *    超声波  VCC→3V3   GND→GND   Trig→GPIO25   Echo→GPIO18
 *    蜂鸣器  VCC→3V3   GND→GND   I/O →GPIO4
 *
 *  功能：
 *    1. 每 60ms 测一次距离，3 帧取中值，过滤单帧毛刺；
 *    2. 按距离分 5 档，越近蜂鸣器节奏越快；
 *    3. 全程用 millis() 非阻塞，不用 delay() 卡住主循环，
 *       以后加跌倒检测、语音、MQTT 时不会互相耽误。
 *
 *  串口 115200，每 0.5 秒打印一次当前距离和档位。
 * ============================================================
 */

// ---------------------- 引脚 ----------------------
const int PIN_TRIG = 25;
const int PIN_ECHO = 18;
const int PIN_BUZZ = 4;

// ---------------------- 测距 ----------------------
unsigned long lastMeasure = 0;
const int MEASURE_GAP = 60;          // 两次测距间隔 60ms

long samples[3] = { -1, -1, -1 };    // 最近 3 次距离
int sampleIdx = 0;
long distCM = -1;                    // 中值距离

// ---------------------- 蜂鸣器节奏 ----------------------
// 五档：0 安静 / 1~4 节奏逐档加快
int level = 0;
int beepInterval = 0;                // 相邻两声"开始"的间隔
const int BEEP_ON = 120;             // 每声持续 120ms

bool buzzing = false;                // 当前是否正在响
unsigned long beepStart = 0;         // 这一声开始的时刻
unsigned long lastBeep = 0;          // 上一声开始的时刻

// ---------------------- 串口打印 ----------------------
unsigned long lastPrint = 0;

// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  digitalWrite(PIN_TRIG, LOW);
  noTone(PIN_BUZZ);

  Serial.println();
  Serial.println("===== 避障分级启动 =====");
  Serial.println("档位：>100 安静 | 50~100 慢 | 20~50 中 | 5~20 快 | <5 急促");
  Serial.println();
}

// ============================================================
void loop() {
  unsigned long now = millis();

  // ---------- 1. 定时测距 ----------
  if (now - lastMeasure >= MEASURE_GAP) {
    lastMeasure = now;
    long cm = readCM();
    if (cm > 0) {                       // 无回波的帧不更新，保留旧值
      samples[sampleIdx] = cm;
      sampleIdx = (sampleIdx + 1) % 3;
      distCM = median3(samples[0], samples[1], samples[2]);
    }
  }

  // ---------- 2. 距离 → 档位 ----------
  if (distCM < 0) {
    level = 0;                          // 还没拿到有效数据，先安静
  } else if (distCM > 100) {
    level = 0;
  } else if (distCM > 50) {
    level = 1;
  } else if (distCM > 20) {
    level = 2;
  } else if (distCM > 5) {
    level = 3;
  } else {
    level = 4;
  }

  // 每档对应两声之间的间隔（毫秒）
  if (level == 0)      beepInterval = 0;
  else if (level == 1) beepInterval = 900;
  else if (level == 2) beepInterval = 550;
  else if (level == 3) beepInterval = 280;
  else                 beepInterval = 150;

  // ---------- 3. 非阻塞蜂鸣器 ----------
  if (level == 0) {
    noTone(PIN_BUZZ);                   // 安全距离，完全安静
    buzzing = false;
  } else {
    if (!buzzing && now - lastBeep >= beepInterval) {
      tone(PIN_BUZZ, 1000);            // 开始响一声
      buzzing = true;
      beepStart = now;
      lastBeep = now;
    }
    if (buzzing && now - beepStart >= BEEP_ON) {
      noTone(PIN_BUZZ);                 // 响够 120ms 就停
      buzzing = false;
    }
  }

  // ---------- 4. 串口打印（每 0.5 秒） ----------
  if (now - lastPrint >= 500) {
    lastPrint = now;
    Serial.print("距离: ");
    if (distCM < 0) Serial.print("--");
    else            Serial.print(distCM);
    Serial.print(" cm | 档位: ");
    Serial.print(level);
    if (level == 0) Serial.println("（安静）");
    else if (level == 4) Serial.println("（急促，危险）");
    else Serial.println();
  }
}

// ============================================================
// 读一次距离，返回厘米；无回波返回 -1
// ============================================================
long readCM() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long d = pulseIn(PIN_ECHO, HIGH, 30000);
  if (d == 0) return -1;
  return d / 58;                        // 每厘米约 58us
}

// ============================================================
// 三数取中值（先排序再取中间）
// ============================================================
long median3(long a, long b, long c) {
  if (a > b) { long t = a; a = b; b = t; }
  if (b > c) { long t = b; b = c; c = t; }
  if (a > b) { long t = a; a = b; b = t; }
  return b;
}
