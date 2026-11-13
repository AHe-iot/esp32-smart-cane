/*
 * ============================================================
 *  H3 · 步骤 8：SYN6288 语音合成功能验证（让它真正开口）
 * ------------------------------------------------------------
 *  接线：
 *      SYN6288 VCC → VIN(5V)      （需要 5V，3.3V 声音极小）
 *      SYN6288 GND → GND          （必须共地）
 *      SYN6288 RXD → GPIO 17      （板子的"发"接模块的"收"）
 *      SYN6288 TXD → 不接
 *      喇叭线    → 模块 SPK 端子
 *
 *  关键：SYN6288 要求中文用 GBK 编码，Arduino 源码默认是 UTF-8，
 *       所以这里直接把 GBK 字节写死（用 PowerShell 按 936 代码页算好）。
 *
 *  帧结构（来自 SYN6288 数据手册）：
 *      0xFD | 长度高 | 长度低 | 0x01 | 0x00 | GBK文本... | 异或校验
 *      数据区长度 = 文本字节数 + 3（命令1 + 编码1 + 校验1）
 *
 *  通过标准：喇叭依次读出「你好 → 前方有障碍 → 测试成功」
 * ============================================================
 */

// 三个短语的 GBK 字节（PowerShell [Encoding]936 计算）
const byte TXT_NIHAO[]   = { 0xC4, 0xE3, 0xBA, 0xC3 };                          // 你好
const byte TXT_OBSTACLE[] = { 0xC7, 0xB0, 0xB7, 0xBD, 0xD3, 0xD0, 0xD5, 0xCF, 0xB0, 0xAD }; // 前方有障碍
const byte TXT_OK[]      = { 0xB2, 0xE2, 0xCA, 0xD4, 0xB3, 0xC9, 0xB9, 0xA6 }; // 测试成功

void setup() {
  Serial.begin(115200);
  // Serial2：默认 RX=16/TX=17，这里只用 TX(17) 往模块发数据
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("\n=== SYN6288 语音合成测试 ===");
  Serial.println("应该依次听到：你好 / 前方有障碍 / 测试成功");
}

void loop() {
  speak(TXT_NIHAO, sizeof(TXT_NIHAO));
  delay(2000);
  speak(TXT_OBSTACLE, sizeof(TXT_OBSTACLE));
  delay(3000);
  speak(TXT_OK, sizeof(TXT_OK));
  delay(4000);
  Serial.println("一轮结束，重新播放...");
}

// 把一段 GBK 文本封装成 SYN6288 帧并发送
void speak(const byte* text, int textLen) {
  byte frame[206];

  frame[0] = 0xFD;                          // 帧头
  int dataLen = textLen + 3;                // 数据区长度 = 文本 + 命令 + 编码 + 校验
  frame[1] = (dataLen >> 8) & 0xFF;         // 长度高字节
  frame[2] = dataLen & 0xFF;                // 长度低字节
  frame[3] = 0x01;                          // 命令字：合成并播放
  frame[4] = 0x00;                          // 编码：GBK

  for (int i = 0; i < textLen; i++) {        // 填入 GBK 文本
    frame[5 + i] = text[i];
  }

  byte xorSum = 0;                           // 异或校验：从帧头到文本逐个 XOR
  int beforeCheck = 5 + textLen;
  for (int i = 0; i < beforeCheck; i++) {
    xorSum ^= frame[i];
  }
  frame[beforeCheck] = xorSum;

  Serial2.write(frame, beforeCheck + 1);     // 总发送 = 6 + 文本长度
}
