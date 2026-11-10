/*
 * DisplayPinFinder - find an unknown QSPI AMOLED's CS / SCK / D0 / read-data pins.
 *
 * For every CS, SCK, D0 combination it bit-bangs the single-line register read
 * (0x03, 0x00, reg, 0x00) and samples all other candidate pins at once. A pin is
 * a hit if it toggles during the reply, repeats it, and stays idle when the same
 * clocks are sent with CS released. Hits then get "all pixels on" for 4 s.
 *
 * Skips flash/PSRAM (26-37), USB (19/20), UART0 (43/44), BOOT (0) and the I2C
 * pins already identified on this board (7-10). All outputs use minimum drive.
 */
#include <Arduino.h>
#include "driver/gpio.h"
#include "soc/gpio_reg.h"

static const int PINS_ALL[] = { 1, 2, 3, 4, 5, 6, 11, 12, 13, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41, 42, 45, 46, 47, 48 };
static const int PINS_NO_PD[] = { 2, 3, 5, 6, 11, 12, 13, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41, 42, 45, 46, 47, 48 };
static const uint8_t REGS[] = { 0x04, 0x0A, 0xDA };
static const int READ_BITS = 32;

static const int *cand;
static int ncand;

struct Hit { int cs, sck, d0, rd; uint8_t reg; uint32_t val; };
static Hit hits[32];
static int nhits = 0;

static inline void pinHi(int p) {
  if (p < 32) REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << p);
  else REG_WRITE(GPIO_OUT1_W1TS_REG, 1UL << (p - 32));
}
static inline void pinLo(int p) {
  if (p < 32) REG_WRITE(GPIO_OUT_W1TC_REG, 1UL << p);
  else REG_WRITE(GPIO_OUT1_W1TC_REG, 1UL << (p - 32));
}
static inline uint64_t readAll() {
  return ((uint64_t)REG_READ(GPIO_IN1_REG) << 32) | REG_READ(GPIO_IN_REG);
}
static inline void toOut(int p) { gpio_set_direction((gpio_num_t)p, GPIO_MODE_OUTPUT); }
static inline void toIn(int p) { gpio_set_direction((gpio_num_t)p, GPIO_MODE_INPUT); }

static void setupPin(int p) {
  gpio_reset_pin((gpio_num_t)p);
  gpio_set_drive_capability((gpio_num_t)p, GPIO_DRIVE_CAP_0);
  gpio_set_direction((gpio_num_t)p, GPIO_MODE_INPUT);
  gpio_set_pull_mode((gpio_num_t)p, GPIO_PULLUP_ONLY);
}

static void sendByte(int sck, int d0, uint8_t b) {
  for (int i = 7; i >= 0; i--) {
    if (b & (1 << i)) pinHi(d0); else pinLo(d0);
    esp_rom_delay_us(1);
    pinHi(sck);
    esp_rom_delay_us(1);
    pinLo(sck);
  }
}

static void readReg(int cs, int sck, int d0, uint8_t reg, bool assertCs, uint64_t *samples) {
  pinHi(cs); pinLo(sck); pinLo(d0);
  toOut(cs); toOut(sck); toOut(d0);
  esp_rom_delay_us(1);
  if (assertCs) pinLo(cs);
  esp_rom_delay_us(1);
  sendByte(sck, d0, 0x03);
  sendByte(sck, d0, 0x00);
  sendByte(sck, d0, reg);
  sendByte(sck, d0, 0x00);
  toIn(d0);
  for (int i = 0; i < READ_BITS; i++) {
    esp_rom_delay_us(1);
    pinHi(sck);
    esp_rom_delay_us(1);
    samples[i] = readAll();
    pinLo(sck);
  }
  pinHi(cs);
  esp_rom_delay_us(1);
  toIn(sck);
  toIn(cs);
}

static uint32_t bitsOf(const uint64_t *samples, int pin) {
  uint32_t v = 0;
  for (int i = 0; i < READ_BITS; i++) v = (v << 1) | ((samples[i] >> pin) & 1);
  return v;
}

static void writeReg(int cs, int sck, int d0, uint8_t reg, int data = -1) {
  pinHi(cs); pinLo(sck);
  toOut(cs); toOut(sck); toOut(d0);
  pinLo(cs);
  esp_rom_delay_us(1);
  sendByte(sck, d0, 0x02);
  sendByte(sck, d0, 0x00);
  sendByte(sck, d0, reg);
  sendByte(sck, d0, 0x00);
  if (data >= 0) sendByte(sck, d0, (uint8_t)data);
  pinHi(cs);
  esp_rom_delay_us(1);
  toIn(sck); toIn(cs); toIn(d0);
}

static bool confirm(int cs, int sck, int d0, int rd, uint8_t reg, uint32_t val) {
  uint64_t s[READ_BITS];
  for (int n = 0; n < 3; n++) {
    readReg(cs, sck, d0, reg, true, s);
    if (bitsOf(s, rd) != val) return false;
  }
  // With CS released a real display must not answer
  readReg(cs, sck, d0, reg, false, s);
  return bitsOf(s, rd) == 0xFFFFFFFF;
}

static void lightTest(const Hit &h) {
  Serial.printf(">>> WHITE TEST 4 s: CS=%d SCK=%d D0=%d\n", h.cs, h.sck, h.d0);
  writeReg(h.cs, h.sck, h.d0, 0x11);
  delay(150);
  writeReg(h.cs, h.sck, h.d0, 0x29);
  writeReg(h.cs, h.sck, h.d0, 0x53, 0x20);
  writeReg(h.cs, h.sck, h.d0, 0x51, 0xFF);
  writeReg(h.cs, h.sck, h.d0, 0x23);
  delay(4000);
  writeReg(h.cs, h.sck, h.d0, 0x22);
}

static void addHit(int cs, int sck, int d0, int rd, uint8_t reg, uint32_t val) {
  for (int i = 0; i < nhits; i++)
    if (hits[i].cs == cs && hits[i].sck == sck && hits[i].d0 == d0 && hits[i].rd == rd) return;
  if (nhits >= 32) return;
  hits[nhits++] = { cs, sck, d0, rd, reg, val };
  Serial.printf("HIT: CS=%d SCK=%d D0=%d READ=%d reg 0x%02X -> 0x%08lX\n", cs, sck, d0, rd, reg, (unsigned long)val);
}

static void tryCombo(int cs, int sck, int d0) {
  uint64_t s[READ_BITS];
  for (uint8_t reg : REGS) {
    readReg(cs, sck, d0, reg, true, s);
    uint64_t any0 = 0, any1 = 0;
    for (int i = 0; i < READ_BITS; i++) { any0 |= ~s[i]; any1 |= s[i]; }
    uint64_t toggled = any0 & any1;
    for (int k = 0; k < ncand; k++) {
      int rd = cand[k];
      if (rd == cs || rd == sck) continue;
      if (!((toggled >> rd) & 1)) continue;
      uint32_t val = bitsOf(s, rd);
      if (confirm(cs, sck, d0, rd, reg, val)) addHit(cs, sck, d0, rd, reg, val);
    }
  }
}

static void pass(bool reverse) {
  uint32_t t0 = millis();
  for (int a = 0; a < ncand; a++) {
    int cs = cand[reverse ? ncand - 1 - a : a];
    for (int b = 0; b < ncand; b++) {
      int sck = cand[reverse ? ncand - 1 - b : b];
      if (sck == cs) continue;
      for (int c = 0; c < ncand; c++) {
        int d0 = cand[reverse ? ncand - 1 - c : c];
        if (d0 == cs || d0 == sck) continue;
        tryCombo(cs, sck, d0);
      }
      yield();
    }
    Serial.printf("  CS %d checked, %lu ms\n", cs, (unsigned long)(millis() - t0));
    delay(150);  // let the panel recover if we just toggled its reset line
  }
}

static void search(const char *title) {
  Serial.printf("\n==== %s (%d pins) ====\n", title, ncand);
  pass(false);
  delay(200);
  pass(true);
}

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("\nDisplayPinFinder start");

  for (int p : PINS_ALL) setupPin(p);
  delay(200);

  // A: GPIO1 and GPIO4 have pull-downs, so they may be power enables: hold them high
  pinMode(1, OUTPUT); digitalWrite(1, HIGH);
  pinMode(4, OUTPUT); digitalWrite(4, HIGH);
  gpio_set_drive_capability(GPIO_NUM_1, GPIO_DRIVE_CAP_0);
  gpio_set_drive_capability(GPIO_NUM_4, GPIO_DRIVE_CAP_0);
  delay(300);
  cand = PINS_NO_PD;
  ncand = sizeof(PINS_NO_PD) / sizeof(PINS_NO_PD[0]);
  search("GPIO1 + GPIO4 held high");

  // B: GPIO1 and GPIO4 may be display signals instead: test them too
  setupPin(1);
  setupPin(4);
  delay(300);
  cand = PINS_ALL;
  ncand = sizeof(PINS_ALL) / sizeof(PINS_ALL[0]);
  search("All candidate pins");

  Serial.printf("\n==== RESULT: %d hit(s) ====\n", nhits);
  for (int i = 0; i < nhits; i++) {
    Serial.printf("CS=%d SCK=%d D0=%d READ=%d reg 0x%02X -> 0x%08lX\n", hits[i].cs, hits[i].sck, hits[i].d0, hits[i].rd,
                  hits[i].reg, (unsigned long)hits[i].val);
  }
  for (int i = 0; i < nhits; i++) lightTest(hits[i]);
  Serial.println("==== finished ====");
}

void loop() {
  delay(1000);
}
