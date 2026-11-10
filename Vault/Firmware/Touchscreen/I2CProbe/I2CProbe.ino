/*
 * I2CProbe - identify which I2C pins / chips a board has.
 *
 * 1. Finds pins with an external pull-up (I2C lines always have one): each pin
 *    gets the weak internal pull-down; if it still reads HIGH it's pulled up.
 * 2. Scans every SDA/SCL pair among those pins and names known chips.
 * 3. Dumps AXP2101 power-output registers if one is found.
 *
 * Skips flash/PSRAM pins (26-37), USB (19/20) and the UART0 console (43/44).
 */
#include <Arduino.h>
#include <Wire.h>

static const int CANDIDATES[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21,
                                  38, 39, 40, 41, 42, 45, 46, 47, 48 };
static const int NCAND = sizeof(CANDIDATES) / sizeof(CANDIDATES[0]);

static int pulled[NCAND];
static int npulled = 0;

static const char *guess(uint8_t a) {
  switch (a) {
    case 0x15: return "CST9217/CST92xx touch?";
    case 0x18: return "ES8311 codec";
    case 0x1A: return "CST816 touch?";
    case 0x20: case 0x21: return "TCA9554 IO expander";
    case 0x34: return "AXP2101 PMIC";
    case 0x38: return "FT3168/FT6x36 touch";
    case 0x40: return "ES7210 mic ADC";
    case 0x51: return "PCF85063 RTC";
    case 0x55: return "SY6970/BQ charger?";
    case 0x5A: return "CST touch / DRV2605 haptic?";
    case 0x6A: case 0x6B: return "QMI8658 IMU";
    default: return "";
  }
}

static uint8_t rd(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return 0xFF;
  return Wire.read();
}

static void dumpAxp() {
  Serial.println("  AXP2101 registers:");
  Serial.printf("    0x03 chip id     = 0x%02X (0x4A = AXP2101)\n", rd(0x34, 0x03));
  Serial.printf("    0x80 DCDC enable = 0x%02X\n", rd(0x34, 0x80));
  Serial.printf("    0x90 LDO enable0 = 0x%02X (b0 ALDO1 b1 ALDO2 b2 ALDO3 b3 ALDO4 b4 BLDO1 b5 BLDO2 b6 CPUSLDO b7 DLDO1)\n",
                rd(0x34, 0x90));
  Serial.printf("    0x91 LDO enable1 = 0x%02X (b0 DLDO2)\n", rd(0x34, 0x91));
  const char *names[] = { "ALDO1", "ALDO2", "ALDO3", "ALDO4", "BLDO1", "BLDO2" };
  for (int i = 0; i < 6; i++) Serial.printf("    %s = %d mV\n", names[i], 500 + (rd(0x34, 0x92 + i) & 0x1F) * 100);
  Serial.printf("    DCDC1 = %d mV\n", 1500 + (rd(0x34, 0x82) & 0x1F) * 100);
}

static void findPullups() {
  Serial.println("\n==== pin levels (internal pull-down applied) ====");
  npulled = 0;
  for (int i = 0; i < NCAND; i++) {
    int p = CANDIDATES[i];
    pinMode(p, INPUT_PULLDOWN);
    delay(2);
    int a = digitalRead(p);
    pinMode(p, INPUT_PULLUP);
    delay(2);
    int b = digitalRead(p);
    pinMode(p, INPUT);
    const char *s = (a && b) ? "PULLED UP" : (!a && !b) ? "pulled down / driven low" : "floating";
    Serial.printf("  GPIO%-2d %s\n", p, s);
    if (a && b && p != 0) pulled[npulled++] = p;
  }
}

static void scanPair(int sda, int scl) {
  Wire.end();
  if (!Wire.begin(sda, scl, 100000)) return;
  Wire.setTimeOut(10);
  int found = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      if (!found) Serial.printf("SDA %d / SCL %d:\n", sda, scl);
      Serial.printf("  0x%02X %s\n", a, guess(a));
      found++;
      if (a == 0x34) dumpAxp();
    }
  }
  Wire.end();
}

static void run() {
  findPullups();
  Serial.printf("\n==== I2C scan over %d pulled-up pins ====\n", npulled);
  for (int i = 0; i < npulled; i++)
    for (int j = 0; j < npulled; j++)
      if (i != j) scanPair(pulled[i], pulled[j]);
  Serial.println("==== done ====");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  run();
}

void loop() {
  delay(8000);
  run();
}
