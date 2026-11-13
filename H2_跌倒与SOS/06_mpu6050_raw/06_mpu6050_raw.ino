/*
 * ============================================================
 *  H2 · 步骤 6：MPU6050 读取六轴原始数据
 * ------------------------------------------------------------
 *  接线（和扫描时一样）：
 *      VCC → 3.3V，GND → GND，SDA → GPIO21，SCL → GPIO22
 *
 *  目的：把 3 轴加速度 + 3 轴陀螺仪读出来，理解每个轴的含义，
 *       为跌倒检测做准备。
 *
 *  关键寄存器（数据手册）：
 *      0x6B 电源管理：写 0 唤醒（上电默认在睡眠，不唤醒读出来全是 0）
 *      0x3B 起：加速度 AX/AY/AZ 各 2 字节（高字节在前）
 *      0x43 起：陀螺仪 GX/GY/GZ 各 2 字节
 *
 *  量程默认：加速度 ±2g（16384 个原始值 = 1g）
 *           陀螺仪 ±250°/s（131 个原始值 = 1°/s）
 *
 *  通过标准：静止放桌面时，朝上的那个轴加速度约 ±1g，其余接近 0，
 *           陀螺仪三轴接近 0；倾斜/移动时数值跟着变化。
 * ============================================================
 */

#include <Wire.h>

const int MPU_ADDR = 0x68;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);                 // 启动 I2C，指定 SDA/SCL

  // 唤醒 MPU6050：向电源管理寄存器 0x6B 写 0
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  Serial.println("\n=== MPU6050 六轴原始数据 ===");
  Serial.println("静止时某轴加速度约 ±1g，陀螺仪接近 0");
}

void loop() {
  // 加速度：从 0x3B 开始连续读 6 字节
  int16_t ax = readReg16(0x3B);
  int16_t ay = readReg16(0x3D);
  int16_t az = readReg16(0x3F);

  // 陀螺仪：从 0x43 开始连续读 6 字节
  int16_t gx = readReg16(0x43);
  int16_t gy = readReg16(0x45);
  int16_t gz = readReg16(0x47);

  // 打印原始值 + 换算后的物理单位
  Serial.print("加速度(g)  X=");
  Serial.print(ax / 16384.0, 2);
  Serial.print("  Y=");
  Serial.print(ay / 16384.0, 2);
  Serial.print("  Z=");
  Serial.print(az / 16384.0, 2);

  Serial.print("   | 陀螺仪(°/s) X=");
  Serial.print(gx / 131.0, 1);
  Serial.print("  Y=");
  Serial.print(gy / 131.0, 1);
  Serial.print("  Z=");
  Serial.println(gz / 131.0, 1);

  delay(300);
}

// 从某个寄存器读 2 字节，拼成一个有符号 16 位整数
int16_t readReg16(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);              // 不释放总线，接着读
  Wire.requestFrom(MPU_ADDR, 2, true);      // 读 2 字节
  int16_t value = (Wire.read() << 8) | Wire.read();  // 高字节在前
  return value;
}
