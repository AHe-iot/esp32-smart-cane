/*
 * ============================================================
 *  H6 · 系统整合 3/3：跌倒检测（MPU6050 + 蜂鸣器）
 * ------------------------------------------------------------
 *  接线：
 *    MPU6050  VCC→3V3  GND→GND  SDA→GPIO21  SCL→GPIO22
 *    蜂鸣器   I/O→GPIO4
 *    按键     GPIO13（用来解除警报，不接也能测）
 *
 *  双判据（防止走路甩手误报）：
 *    1. 冲击：加速度合矢量瞬间 > 2.5 g；
 *    2. 姿态：冲击后 1.2 秒内模块仍处于横躺（倾角 > 45°）。
 *    两条都满足才确认跌倒，蜂鸣器急促报警；按一下按键解除。
 *
 *  全程 millis() 非阻塞；串口 115200 打印合矢量、倾角和状态。
 * ============================================================
 */

#include <Wire.h>

const int PIN_BUZZ   = 4;
const int PIN_BUTTON = 13;

// ---------------------- 阈值（方便以后调） ----------------------
const float IMPACT_G = 2.5;      // 冲击阈值 g
const float TILT_DEG = 45.0;     // 横躺判定角
const int CHECK_MS   = 1200;     // 冲击后观察窗口

// ---------------------- 状态机 ----------------------
enum FallState { NORMAL, CHECK, FALL_ALARM };
int state = NORMAL;
unsigned long stateStart = 0;

// ---------------------- 数据 ----------------------
float gx = 0, gy = 0, gz = 0;     // 三轴加速度，单位 g
float mag = 0;                    // 合矢量 g
float angle = 0;                  // 与竖直方向夹角，度

unsigned long lastRead = 0;
const int READ_GAP = 25;          // 25ms 读一次
unsigned long lastPrint = 0;

// 报警蜂鸣（非阻塞）
bool buzzing = false;
unsigned long beepStart = 0, lastBeep = 0;
const int ALARM_GAP = 150;
const int ALARM_ON = 110;

// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  noTone(PIN_BUZZ);

  Wire.begin(21, 22);
  // 唤醒芯片
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  Serial.println();
  Serial.println("===== 跌倒检测启动 =====");
  Serial.println("测试：先快速甩一下（冲击），再把模块横放 1.2 秒");
}

// ============================================================
void loop() {
  unsigned long now = millis();

  // ---------- 1. 定时读加速度 ----------
  if (now - lastRead >= READ_GAP) {
    lastRead = now;
    readAccel();
    mag = sqrt(gx * gx + gy * gy + gz * gz);
    // 与竖直方向夹角：atan2(水平分量, 竖直分量)
    angle = atan2(sqrt(gx * gx + gy * gy), gz) * 180.0 / PI;
  }

  // ---------- 2. 状态机 ----------
  if (state == NORMAL) {
    if (mag > IMPACT_G) {
      state = CHECK;
      stateStart = now;
      Serial.println(">>> 检测到冲击，进入 1.2 秒姿态观察");
    }
  } else if (state == CHECK) {
    if (now - stateStart >= CHECK_MS) {
      if (angle > TILT_DEG) {
        state = FALL_ALARM;
        stateStart = now;
        Serial.println("!!! 确认跌倒：持续横躺，启动报警（按按键解除）");
      } else {
        state = NORMAL;
        Serial.println("    已恢复竖直，判定为误报，解除");
      }
    }
  } else if (state == FALL_ALARM) {
    // 按按键解除
    if (digitalRead(PIN_BUTTON) == LOW) {
      state = NORMAL;
      noTone(PIN_BUZZ);
      buzzing = false;
      Serial.println("<<< 手动解除报警");
    }
  }

  // ---------- 3. 报警声（非阻塞） ----------
  if (state == FALL_ALARM) {
    if (!buzzing && now - lastBeep >= ALARM_GAP) {
      tone(PIN_BUZZ, 1500);
      buzzing = true;
      beepStart = now;
      lastBeep = now;
    }
    if (buzzing && now - beepStart >= ALARM_ON) {
      noTone(PIN_BUZZ);
      buzzing = false;
    }
  }

  // ---------- 4. 打印 ----------
  if (now - lastPrint >= 300) {
    lastPrint = now;
    Serial.print("合矢量: "); Serial.print(mag, 2);
    Serial.print(" g | 倾角: "); Serial.print(angle, 1);
    Serial.print("° | 状态: ");
    if (state == NORMAL) Serial.println("正常");
    else if (state == CHECK) Serial.println("观察中");
    else Serial.println("跌倒报警");
  }
}

// ============================================================
// 读 6 字节加速度，换算成 g（±2g 量程：16384 LSB/g）
// ============================================================
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
