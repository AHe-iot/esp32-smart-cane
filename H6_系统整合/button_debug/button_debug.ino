/*
 * 按键电平调试：每 200ms 打印 GPIO13 的原始电平
 * 正常：松开打印 1，按下打印 0
 */
const int PIN_BUTTON = 13;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  Serial.println("按键调试：松开应=1，按下应=0");
}

void loop() {
  Serial.println(digitalRead(PIN_BUTTON));
  delay(200);
}
