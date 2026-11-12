/*
 * ============================================================
 *  测试 1 · 蜂鸣器验证
 * ------------------------------------------------------------
 *  目的：确认你买到的是「无源」蜂鸣器，并且能被 tone() 调音
 *  接线：VCC → 3.3V    GND → GND    I/O → GPIO 4
 *  预期：听到「低音 → 高音 → 更高音」三个明显不同的音
 * ============================================================
 */

const int PIN_BUZZER = 4;

// 低电平触发模块：HIGH = 三极管关断 = 不响
// 如果烧进去之后蜂鸣器一直响不停，把下面这行改成 LOW
#define BUZZER_OFF  HIGH

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, BUZZER_OFF);   // 先确保是关的
  Serial.println("\n=== 蜂鸣器测试开始 ===");
  Serial.println("应该听到三个明显不同的音高");
}

void loop() {
  Serial.println("400 Hz  (低)");
  tone(PIN_BUZZER, 400);
  delay(500);
  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, BUZZER_OFF);
  delay(300);

  Serial.println("800 Hz  (高)");
  tone(PIN_BUZZER, 800);
  delay(500);
  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, BUZZER_OFF);
  delay(300);

  Serial.println("2000 Hz (更高)");
  tone(PIN_BUZZER, 2000);
  delay(500);
  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, BUZZER_OFF);
  delay(300);

  Serial.println("--- 一轮结束，1.5 秒后重来 ---\n");
  delay(1500);
}

/*
 * ============================================================
 *  结果判断
 * ------------------------------------------------------------
 *  ✅ 三个音高明显不同，中间有停顿
 *     → 买对了，是无源蜂鸣器，可以继续
 *
 *  ❌ 三个音完全一样
 *     → 买成有源蜂鸣器了，tone() 调不了频率
 *       可以继续用，但五档分级只能靠「节奏」区分，不能靠音高
 *
 *  ❌ 一直响不停
 *     → 极性搞反了，把 BUZZER_OFF 改成 LOW
 *
 *  ❌ 只有「嗒、嗒」的开关声，没有持续的音
 *     → 有源蜂鸣器 + 极性不对，先试改 LOW，还不行就是买错了
 *
 *  ❌ 完全不响
 *     → 先检查 I/O 是不是接到了 GPIO 4
 *       再检查 GND 有没有和 ESP32 共地
 *       最后把 2000Hz 换成 2700Hz 试试（有些蜂鸣器谐振点在 2.7k）
 * ============================================================
 */
