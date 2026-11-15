/*
 * ============================================================
 *  H6 · 系统整合 2/3：SOS 按键求助（按键 + 蜂鸣器）
 * ------------------------------------------------------------
 *  接线：
 *    按键    一脚→GPIO13   对角脚→GND（INPUT_PULLUP，按下为 LOW）
 *    蜂鸣器  I/O →GPIO4
 *
 *  功能：
 *    1. 按下按键（消抖 30ms，只认一次按下，不重复触发）；
 *    2. 蜂鸣器用摩斯码播放 SOS：三短 · 三长 · 三短；
 *    3. 全程 millis() 非阻塞状态机，播放期间不卡主循环。
 *
 *  以后这一步会接上：MQTT 发求助消息、SYN6288 播报"正在求助"。
 *  串口 115200。
 * ============================================================
 */

const int PIN_BUTTON = 13;
const int PIN_BUZZ   = 4;

// ---------------------- 按键消抖 ----------------------
int lastRaw = HIGH;
int stableState = HIGH;
unsigned long lastBounce = 0;
const int DEBOUNCE_MS = 30;
bool sosStarted = false;        // 防止按住不放重复触发

// ---------------------- SOS 状态机 ----------------------
// 9 个元素：前 3 短、中 3 长、后 3 短（单位 ms）
const int DOT = 150;
const int DASH = 450;
const int elemDur[9] = { DOT, DOT, DOT, DASH, DASH, DASH, DOT, DOT, DOT };

enum SosPhase { IDLE, TONE_ON, GAP_OFF };
int phase = IDLE;
int elemIndex = 0;
unsigned long phaseStart = 0;

// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZ, OUTPUT);
  noTone(PIN_BUZZ);

  Serial.println();
  Serial.println("===== SOS 按键求助启动 =====");
  Serial.println("按一下按键，应听到：三短 · 三长 · 三短");
}

// ============================================================
void loop() {
  unsigned long now = millis();

  // ---------- 1. 按键消抖 ----------
  int raw = digitalRead(PIN_BUTTON);
  if (raw != lastRaw) {
    lastBounce = now;          // 电平刚变化，重新计时
    lastRaw = raw;
  }
  if (now - lastBounce >= DEBOUNCE_MS && raw != stableState) {
    stableState = raw;         // 稳定 30ms 才采纳
  }

  // ---------- 2. 检测"按下"事件（下降沿，只触发一次） ----------
  if (stableState == LOW && !sosStarted && phase == IDLE) {
    sosStarted = true;
    Serial.println(">>> SOS 按键触发，开始播放摩斯码");
    phase = TONE_ON;
    elemIndex = 0;
    phaseStart = now;
    tone(PIN_BUZZ, 1200);
  }
  // 松开后才允许下一次触发
  if (stableState == HIGH) {
    sosStarted = false;
  }

  // ---------- 3. SOS 非阻塞状态机 ----------
  if (phase == TONE_ON) {
    if (now - phaseStart >= elemDur[elemIndex]) {
      noTone(PIN_BUZZ);
      phase = GAP_OFF;
      phaseStart = now;
    }
  } else if (phase == GAP_OFF) {
    // 元素之间停 150ms；字母之间（第 3、第 6 个元素后）停 300ms
    int gap = (elemIndex == 2 || elemIndex == 5) ? 300 : 150;
    if (now - phaseStart >= gap) {
      elemIndex++;
      if (elemIndex >= 9) {
        phase = IDLE;
        noTone(PIN_BUZZ);
        Serial.println("<<< SOS 播放结束，等待下一次按键");
      } else {
        phase = TONE_ON;
        phaseStart = now;
        tone(PIN_BUZZ, 1200);
      }
    }
  }
}
