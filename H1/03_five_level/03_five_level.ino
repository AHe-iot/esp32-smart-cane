/*
 * ============================================================
 *  H1 · 五档分级避障提示（真机版）
 * ------------------------------------------------------------
 *  硬件：ESP32-WROOM-32 + HC-SR04 + 无源蜂鸣器模块
 *  接线：
 *    超声波 VCC  → 3.3V      ⚠️ 不要接 5V
 *    超声波 GND  → GND
 *    超声波 Trig → GPIO 25
 *    超声波 Echo → GPIO 18
 *    蜂鸣器 VCC  → 3.3V
 *    蜂鸣器 GND  → GND
 *    蜂鸣器 I/O  → GPIO 4
 *
 *  串口波特率：115200
 *
 *  相比仿真版，这一版改了三件事：
 *    ① 滤波从「双条件判断」改成「三帧中值」—— 更稳，标定实验时不会误判
 *    ② 蜂鸣节奏从 delay() 改成 millis() —— 响的时候还在测距，没有感知盲区
 *    ③ 增加了原始数据打印 —— 为标定实验和排障准备
 * ============================================================
 */

// ==================== 1. 引脚定义 ====================
const int PIN_TRIG   = 25;
const int PIN_ECHO   = 18;
const int PIN_BUZZER = 4;

// 低电平触发模块：HIGH = 三极管关断
// 如果蜂鸣器一直响不停，改成 LOW
#define BUZZER_OFF  HIGH

// ==================== 2. 五档阈值（单位 cm）====================
const float DIST_SAFE = 100.0;   // > 100 静音
const float DIST_SLOW = 50.0;    // 50 ~ 100 慢滴
const float DIST_MID  = 20.0;    // 20 ~ 50  中速
const float DIST_FAST = 5.0;     // 5 ~ 20   快滴
                                 // <= 5     长鸣

// ==================== 3. 各档音高与间隔 ====================
// 如果实测觉得声音太小，把下面四个频率整体改成 2000/2500/3000/3500
// 原因：无源蜂鸣器的谐振点通常在 2~4kHz，400~800Hz 驱动时音量偏小
const int FREQ_SLOW   = 400;
const int FREQ_MID    = 500;
const int FREQ_FAST   = 650;
const int FREQ_DANGER = 800;

const int GAP_SLOW = 600;   // 两声之间的间隔（ms）
const int GAP_MID  = 350;
const int GAP_FAST = 150;

const int BEEP_MS = 80;     // 每声持续多久（ms）

// ==================== 4. 档位枚举 ====================
enum Level { LV_SAFE = 0, LV_SLOW, LV_MID, LV_FAST, LV_DANGER };

const char* LEVEL_NAME[] = { "SAFE", "SLOW", "MID", "FAST", "DANGER" };

// ==================== 5. 蜂鸣器状态 ====================
bool          beeping   = false;
unsigned long beepStart = 0;   // 这一声是什么时候开始的
unsigned long lastStop  = 0;   // 上一声是什么时候结束的

// ==================== 6. setup ====================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, BUZZER_OFF);

  Serial.println("\n\n=== H1 五档分级避障（真机版）===");
  Serial.println("把障碍物从远处慢慢靠近，观察蜂鸣节奏变化\n");
  Serial.println("dist(cm)\tlevel");
  Serial.println("--------------------------------");
}

// ==================== 7. 主循环 ====================
void loop() {
  float d = readDistance();       // 取一次滤波后的距离
  int   lv = getLevel(d);         // 判档

  handleBuzzer(lv);               // 控制蜂鸣（非阻塞）
  printDebug(d, lv);              // 打印（标定实验用）

  // 主循环不加 delay，让蜂鸣节奏靠 millis 控制
}

// ==================== 8. 单次测距（最原始）====================
// 返回 -1 表示没有回波
float readOnce() {
  // 发 5μs 触发脉冲
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(5);
  digitalWrite(PIN_TRIG, LOW);

  // 测回波高电平时长，超时 20000μs
  unsigned long dur = pulseIn(PIN_ECHO, HIGH, 20000);

  if (dur == 0) return -1.0;            // 超时 = 没收到回波
  return dur * 0.034 / 2.0;             // d = t × 声速 / 2
}

// ==================== 9. 三帧中值滤波 ====================
/*
 * 为什么用中值，不用你仿真版的双条件判断？
 *
 * 双条件版本：if (d > last + 80 && last < 15) d = last;
 *   它解决的问题：近距离盲区会突然跳到 300+
 *   它的漏洞：如果真实距离从很近（<15cm）快速变远，
 *             也会被判成盲区假值，被压回去 —— 标定实验时手一拿远就卡住
 *
 * 中值版本：连测 3 次，排序取中间那个
 *   单次的毛刺（包括盲区的 300+）会被自动丢掉
 *   真实的变化（连续 3 次都变大）会正常保留
 *   不需要「猜阈值」，逻辑更简单也更可靠
 *
 * 代价：一次测量要 3 个来回，约 75ms。后面用 FreeRTOS 可以并行优化。
 */
float readDistance() {
  float v[3];
  int   n = 0;

  for (int i = 0; i < 3; i++) {
    float d = readOnce();
    if (d > 0) v[n++] = d;      // 只收集有效读数
  }

  if (n == 0) return -1.0;                       // 三次都没回波 = 空旷
  if (n == 1) return v[0];                       // 只有一个有效，就用它
  if (n == 2) return (v[0] + v[1]) / 2.0;        // 两个有效，取平均

  // 三个有效，排序取中值（冒泡三行搞定）
  if (v[0] > v[1]) { float t = v[0]; v[0] = v[1]; v[1] = t; }
  if (v[1] > v[2]) { float t = v[1]; v[1] = v[2]; v[2] = t; }
  if (v[0] > v[1]) { float t = v[0]; v[0] = v[1]; v[1] = t; }
  return v[1];
}

// ==================== 10. 判档 ====================
int getLevel(float d) {
  if (d < 0)           return LV_SAFE;    // 没回波 = 前方空旷
  if (d > DIST_SAFE)   return LV_SAFE;
  if (d > DIST_SLOW)   return LV_SLOW;
  if (d > DIST_MID)    return LV_MID;
  if (d > DIST_FAST)   return LV_FAST;
  return LV_DANGER;
}

// ==================== 11. 蜂鸣控制（非阻塞）====================
/*
 * 为什么不用 delay()？
 *
 * 仿真版是：tone(); delay(600);  → 这 600ms 里完全没有测距
 * 以 1.2m/s 的步速算，600ms 能走 72cm —— 正好是一个危险距离
 * 也就是说「响得最急的时候，反而最看不见」，逻辑上是反的
 *
 * 这一版用 millis() 记时间戳，响完就返回，主循环继续测距
 */
void handleBuzzer(int lv) {
  unsigned long now = millis();

  // ---- 第一步：到时间就停 ----
  if (beeping && (now - beepStart >= BEEP_MS)) {
    stopBeep();
    lastStop = now;
  }

  // ---- 第二步：判断现在该不该响 ----
  if (lv == LV_SAFE) return;              // 安全，静音

  if (lv == LV_DANGER) {                  // 危险，连续长鸣
    if (!beeping) startBeep(FREQ_DANGER);
    return;
  }

  int freq, gap;
  if (lv == LV_SLOW)      { freq = FREQ_SLOW; gap = GAP_SLOW; }
  else if (lv == LV_MID)  { freq = FREQ_MID;  gap = GAP_MID;  }
  else                    { freq = FREQ_FAST; gap = GAP_FAST; }

  // 只有在「没在响」且「距上一声结束够久」时才响下一声
  if (!beeping && (now - lastStop >= (unsigned long)gap)) {
    startBeep(freq);
  }
}

void startBeep(int freq) {
  tone(PIN_BUZZER, freq);
  beeping   = true;
  beepStart = millis();
}

void stopBeep() {
  noTone(PIN_BUZZER);
  // 关键：低电平触发的模块，noTone 之后引脚可能是低电平，
  // 那会让三极管一直导通、蜂鸣器通直流 —— 不响但费电还发热
  // 所以必须显式拉高，确保关断
  digitalWrite(PIN_BUZZER, BUZZER_OFF);
  beeping = false;
}

// ==================== 12. 调试打印 ====================
void printDebug(float d, int lv) {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < 200) return;   // 200ms 打一次，别刷太快
  lastPrint = millis();

  if (d < 0) {
    Serial.print("--\t\t");
  } else {
    Serial.print(d, 1);
    Serial.print("\t\t");
  }
  Serial.println(LEVEL_NAME[lv]);
}

/*
 * ============================================================
 *  烧进去之后怎么验证
 * ------------------------------------------------------------
 *  1. 手放在 150cm 外     → 屏幕显示 SAFE，蜂鸣器静音
 *  2. 慢慢靠近到 80cm     → 变 SLOW，听到「滴……滴……滴」慢速
 *  3. 靠近到 35cm         → 变 MID，节奏明显加快
 *  4. 靠近到 12cm         → 变 FAST，「滴滴滴滴」很急
 *  5. 贴到 3cm            → 变 DANGER，连续长鸣
 *  6. 快速拿远            → 立刻回到 SAFE，不会卡住
 *
 *  第 6 步是关键：这是双条件滤波会失败、中值滤波能通过的地方
 *
 * ============================================================
 *  常见故障
 * ------------------------------------------------------------
 *  ❌ 一直 SAFE，靠近也不变
 *     → 超声波没接对，先用 02 号测试单独验证
 *
 *  ❌ 一直 DANGER，长鸣不停
 *     → 读数一直是 0 或很小，说明 pulseIn 返回 0 被你当成 0cm 了
 *       检查：readOnce 里 dur==0 时返回 -1，如果代码被改过要核对
 *     → 也可能是 Echo 没接到 GPIO 18
 *
 *  ❌ 蜂鸣器一直响，不受控制
 *     → BUZZER_OFF 改 LOW（极性反了）
 *
 *  ❌ 节奏对但声音很小
 *     → 把 FREQ_SLOW/MID/FAST/DANGER 改成 2000/2500/3000/3500
 *
 *  ❌ 靠近时数值在 20 和 300 之间来回跳
 *     → 中值滤波只能滤掉单次毛刺。如果连续 2 次都是 300+，
 *       说明盲区是持续性的，需要再加一条「<2cm 直接判 DANGER」的规则
 *
 *  ❌ 串口全是乱码
 *     → 波特率选成 9600 了，改成 115200
 * ============================================================
 */
