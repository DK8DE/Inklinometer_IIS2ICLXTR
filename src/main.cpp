/**
 * IIS2ICLX – Antennen-Elevation + RS485 GET/SET-Protokoll
 * ESP32-C3 | SDA=GPIO4 | SCL=GPIO5 | INT1=GPIO6 | INT2=GPIO7
 * RS485: UART0 TX=GPIO21 RX=GPIO20 @ 115200
 * USB-CDC: optionaler Debug (SETDEBUG)
 * GPIO0: Boot=Werkreset, Laufzeit=SoftAP-Web-UI Toggle
 */

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <esp_mac.h>
#include <nvs_flash.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "app_api.h"
#include "web_ui.h"

static constexpr int PIN_SDA = 4;
static constexpr int PIN_SCL = 5;
static constexpr int PIN_INT1 = 6;  // IIS2ICLX INT1 → FIFO watermark
static constexpr int PIN_INT2 = 7;  // IIS2ICLX INT2 → sleep/stationary status
static constexpr int PIN_RS485_RX = 20;
static constexpr int PIN_RS485_TX = 21;
static constexpr int PIN_LBLED = 10;  // Lebensblink-LED
static constexpr int PIN_BTN = 0;     // Taster gegen GND (+ intern/extern Pull-up)
static constexpr uint8_t ADDR = 0x6B;

static constexpr char SOFTAP_PASS[] = "Rotorconfig";
static constexpr uint32_t BTN_DEBOUNCE_MS = 40;
static constexpr uint16_t FIFO_WTM = 8;  // ~77 ms @ 104 Hz (Latenz vs. Rauschen)
static constexpr uint8_t FIFO_TAG_XL = 2;  // IIS2ICLX_XL_NC_TAG

static constexpr uint8_t REG_FIFO_CTRL1 = 0x07;
static constexpr uint8_t REG_FIFO_CTRL2 = 0x08;
static constexpr uint8_t REG_FIFO_CTRL3 = 0x09;
static constexpr uint8_t REG_FIFO_CTRL4 = 0x0A;
static constexpr uint8_t REG_INT1_CTRL = 0x0D;
static constexpr uint8_t REG_INT2_CTRL = 0x0E;
static constexpr uint8_t REG_WHO_AM_I = 0x0F;
static constexpr uint8_t REG_CTRL1_XL = 0x10;
static constexpr uint8_t REG_CTRL3_C = 0x12;
static constexpr uint8_t REG_CTRL8_XL = 0x17;
static constexpr uint8_t REG_CTRL9_XL = 0x18;
static constexpr uint8_t REG_STATUS = 0x1E;
static constexpr uint8_t REG_OUTX_L_A = 0x28;
static constexpr uint8_t REG_FIFO_STATUS1 = 0x3A;
static constexpr uint8_t REG_FIFO_STATUS2 = 0x3B;
static constexpr uint8_t REG_TAP_CFG0 = 0x56;
static constexpr uint8_t REG_TAP_CFG2 = 0x58;
static constexpr uint8_t REG_WAKE_UP_THS = 0x5B;
static constexpr uint8_t REG_WAKE_UP_DUR = 0x5C;
static constexpr uint8_t REG_MD1_CFG = 0x5E;
static constexpr uint8_t REG_MD2_CFG = 0x5F;
static constexpr uint8_t REG_FIFO_DATA_OUT_TAG = 0x78;

static constexpr uint8_t WHO_AM_I_VAL = 0x6B;
// CTRL1_XL: ODR=104 Hz, FS, LPF2_XL_EN=1
static constexpr uint8_t CTRL1_XL_FS_2G = 0x4E;  // FS=±2 g
static constexpr uint8_t CTRL1_XL_FS_1G = 0x4A;  // FS=±1 g
// LPF2 HPCF=010 → ODR/20 (~5 Hz @ 104 Hz). ODR/800 war zu träge (Sekunden-Nachlauf).
static constexpr uint8_t CTRL8_XL_LPF_ODR_DIV20 = 0x40;
static constexpr uint8_t WAKE_UP_THS_VAL = 0x04;       // ~FS/2^6 * 4
static constexpr uint8_t WAKE_UP_DUR_VAL = 0x20;       // wake_dur=1 (2 ODR), sleep_dur=0 (16 ODR)
static constexpr float SENS_2G_MG_LSB = 0.061f;
static constexpr float SENS_1G_MG_LSB = 0.031f;

static constexpr size_t PROTO_BUF_MAX = 96;

// --- Compile-Defaults (Werkreset) ---
static constexpr bool DEF_SWAP_XY = true;
static constexpr bool DEF_INVERT_REF = false;
static constexpr bool DEF_INVERT_SENSE = true;
static constexpr bool DEF_INVERT_ROT = true;
static constexpr bool DEF_LIMIT_180 = false;
static constexpr float DEF_CALIB = 0.0f;
static constexpr int DEF_MOUNT_ROT = 0;  // 0 / 90 / 180 / 270
static constexpr int DEF_ELEV_DEC = 2;
static constexpr bool DEF_FS_1G = true;
static constexpr float DEF_FILTER = 0.15f;
static constexpr bool DEF_DEBUG = false;
static constexpr char DEF_UI_LANG[] = "de";

// --- Laufzeit-Konfiguration (NVS) ---
bool swapXY = DEF_SWAP_XY;
bool invertRef = DEF_INVERT_REF;
bool invertSense = DEF_INVERT_SENSE;
bool invertRotationDir = DEF_INVERT_ROT;
bool limitElevation0to180 = DEF_LIMIT_180;
float calibOffsetDeg = DEF_CALIB;
int mountRotationDeg = DEF_MOUNT_ROT;
int elevDecimals = DEF_ELEV_DEC;
bool fullScale1g = DEF_FS_1G;
float filterAlpha = DEF_FILTER;  // 0.01…1.0; größer = schneller
bool debugSerial = DEF_DEBUG;    // USB-CDC Debug
char uiLang[4] = "de";           // Web-UI: "de" / "en"
bool g_errorActive = false;      // true → LBLED dauerhaft an

static float g_elevFiltered = NAN;
static bool g_appliedFs1g = false;
static uint32_t g_liveSeq = 0;
static bool g_settled = true;    // INT2 sleep/stationary status
static bool g_fifoOk = false;    // FIFO+INT1 configured
static Preferences g_prefs;
static HardwareSerial RS485(0);

static char g_rxBuf[PROTO_BUF_MAX];
static size_t g_rxLen = 0;
static bool g_rxActive = false;

static bool g_btnLastRaw = true;
static bool g_btnLastStable = true;
static uint32_t g_btnLastChangeMs = 0;

// ---------------------------------------------------------------------------
// I2C / Sensor
// ---------------------------------------------------------------------------

static bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission(true) == 0;
}

static bool readRegs(uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(true) != 0) {
    return false;
  }
  if (Wire.requestFrom((int)ADDR, (int)len, (int)true) != (int)len) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

static bool readReg(uint8_t reg, uint8_t &val) {
  return readRegs(reg, &val, 1);
}

static bool softReset() {
  if (!writeReg(REG_CTRL3_C, 0x01)) {
    return false;
  }
  for (int i = 0; i < 50; i++) {
    uint8_t v = 0;
    if (!readReg(REG_CTRL3_C, v)) {
      return false;
    }
    if ((v & 0x01) == 0) {
      return true;
    }
    delay(10);
  }
  return false;
}

static uint8_t ctrl1XlForFs() {
  return fullScale1g ? CTRL1_XL_FS_1G : CTRL1_XL_FS_2G;
}

static float sensMgPerLsb() {
  return fullScale1g ? SENS_1G_MG_LSB : SENS_2G_MG_LSB;
}

static bool configureFifoAndInterrupts() {
  // FIFO: watermark 16, batch XL @ 104 Hz, continuous (stream) mode
  if (!writeReg(REG_FIFO_CTRL1, (uint8_t)(FIFO_WTM & 0xFF))) {
    return false;
  }
  if (!writeReg(REG_FIFO_CTRL2, (uint8_t)((FIFO_WTM >> 8) & 0x01))) {
    return false;
  }
  if (!writeReg(REG_FIFO_CTRL3, 0x04)) {  // BDR_XL = 104 Hz
    return false;
  }
  if (!writeReg(REG_FIFO_CTRL4, 0x06)) {  // STREAM / continuous
    return false;
  }
  if (!writeReg(REG_INT1_CTRL, 0x08)) {  // INT1_FIFO_TH
    return false;
  }
  if (!writeReg(REG_INT2_CTRL, 0x00)) {
    return false;
  }

  // Wake-up / stationary → INT2 as sleep status level
  if (!writeReg(REG_WAKE_UP_THS, WAKE_UP_THS_VAL)) {
    return false;
  }
  if (!writeReg(REG_WAKE_UP_DUR, WAKE_UP_DUR_VAL)) {
    return false;
  }
  if (!writeReg(REG_TAP_CFG0, 0x20)) {  // SLEEP_STATUS_ON_INT
    return false;
  }
  if (!writeReg(REG_TAP_CFG2, 0x80)) {  // INTERRUPTS_ENABLE
    return false;
  }
  if (!writeReg(REG_MD1_CFG, 0x00)) {
    return false;
  }
  if (!writeReg(REG_MD2_CFG, 0x80)) {  // INT2_SLEEP_CHANGE
    return false;
  }
  return true;
}

bool applyFullScale() {
  if (!writeReg(REG_CTRL1_XL, ctrl1XlForFs())) {
    g_errorActive = true;
    return false;
  }
  g_appliedFs1g = fullScale1g;
  delay(20);
  return true;
}

static bool initSensor() {
  uint8_t who = 0;
  g_fifoOk = false;
  if (!readReg(REG_WHO_AM_I, who) || who != WHO_AM_I_VAL) {
    return false;
  }
  if (!softReset()) {
    return false;
  }
  delay(20);
  if (!writeReg(REG_CTRL9_XL, 0xE1)) {
    return false;
  }
  if (!writeReg(REG_CTRL3_C, 0x44)) {  // BDU + IF_INC, INT active-high
    return false;
  }
  if (!writeReg(REG_CTRL8_XL, CTRL8_XL_LPF_ODR_DIV20)) {
    return false;
  }
  if (!applyFullScale()) {
    return false;
  }
  if (!configureFifoAndInterrupts()) {
    return false;
  }
  g_fifoOk = true;
  delay(50);
  return true;
}

static void updateSettledFromInt2() {
  // SLEEP_STATUS_ON_INT: HIGH = sleep/stationary (settled)
  g_settled = digitalRead(PIN_INT2) == HIGH;
}

bool isSettled() {
  updateSettledFromInt2();
  return g_settled;
}

static float wrap360(float deg) {
  while (deg < 0.0f) {
    deg += 360.0f;
  }
  while (deg >= 360.0f) {
    deg -= 360.0f;
  }
  return deg;
}

static float clampOffset180(float offset) {
  if (offset > 180.0f) {
    offset = 180.0f;
  }
  if (offset < -180.0f) {
    offset = -180.0f;
  }
  return offset;
}

static float applyLimit0to180(float elev360) {
  elev360 = wrap360(elev360);
  if (elev360 > 180.0f && elev360 <= 270.0f) {
    return 180.0f;
  }
  if (elev360 > 270.0f) {
    return 0.0f;
  }
  return elev360;
}

static float angleDiffDeg(float target, float current) {
  float d = target - current;
  if (d > 180.0f) {
    d -= 360.0f;
  }
  if (d < -180.0f) {
    d += 360.0f;
  }
  return d;
}

static float elevationFromAccel(float ax, float ay, bool raw) {
  float ref = swapXY ? ax : ay;
  float sense = swapXY ? ay : ax;
  if (invertRef) {
    ref = -ref;
  }
  if (invertSense) {
    sense = -sense;
  }

  float elev = atan2f(sense, ref) * (180.0f / PI);
  elev = wrap360(elev);

  if (invertRotationDir) {
    elev = wrap360(-elev);
  }

  elev = wrap360(elev + (float)mountRotationDeg);

  if (!raw) {
    elev = wrap360(elev + clampOffset180(calibOffsetDeg));
  }
  return elev;
}

static bool readElevation(float &elevationDeg, bool raw = false) {
  uint8_t status = 0;
  if (!readReg(REG_STATUS, status) || (status & 0x01) == 0) {
    return false;
  }

  uint8_t buf[4];
  if (!readRegs(REG_OUTX_L_A, buf, 4)) {
    return false;
  }

  int16_t rawX = (int16_t)((buf[1] << 8) | buf[0]);
  int16_t rawY = (int16_t)((buf[3] << 8) | buf[2]);

  float ax = rawX * sensMgPerLsb() / 1000.0f;
  float ay = rawY * sensMgPerLsb() / 1000.0f;
  elevationDeg = elevationFromAccel(ax, ay, raw);
  return true;
}

/** FIFO: ältere Samples verwerfen, nur die letzten N mitteln (ax/ay → atan2). */
static bool readElevationFromFifo(float &elevationDeg, bool raw = false) {
  uint8_t st[2];
  if (!readRegs(REG_FIFO_STATUS1, st, 2)) {
    return false;
  }
  uint16_t unread = (uint16_t)st[0] | ((uint16_t)(st[1] & 0x03) << 8);
  if (unread == 0) {
    return false;
  }

  // Rückstau verwerfen – sonst mittelt man Sekunden alte Werte mit
  int skip = (int)unread - (int)FIFO_WTM;
  if (skip < 0) {
    skip = 0;
  }
  for (int i = 0; i < skip; i++) {
    uint8_t word[7];
    if (!readRegs(REG_FIFO_DATA_OUT_TAG, word, 7)) {
      return false;
    }
  }

  float sumAx = 0;
  float sumAy = 0;
  int n = 0;
  const int want = (int)unread - skip;
  for (int i = 0; i < want; i++) {
    uint8_t word[7];
    if (!readRegs(REG_FIFO_DATA_OUT_TAG, word, 7)) {
      break;
    }
    const uint8_t tag = (uint8_t)((word[0] >> 3) & 0x1F);
    if (tag != FIFO_TAG_XL) {
      continue;
    }
    int16_t rawX = (int16_t)((word[2] << 8) | word[1]);
    int16_t rawY = (int16_t)((word[4] << 8) | word[3]);
    sumAx += rawX * sensMgPerLsb() / 1000.0f;
    sumAy += rawY * sensMgPerLsb() / 1000.0f;
    n++;
  }

  if (n < 1) {
    return false;
  }
  elevationDeg = elevationFromAccel(sumAx / (float)n, sumAy / (float)n, raw);
  return true;
}

float currentElevationOut() {
  float out = isnan(g_elevFiltered) ? 0.0f : g_elevFiltered;
  if (limitElevation0to180) {
    out = applyLimit0to180(out);
  }
  int dec = elevDecimals;
  if (dec < 0) {
    dec = 0;
  }
  if (dec > 2) {
    dec = 2;
  }
  float scale = powf(10.0f, (float)dec);
  return roundf(out * scale) / scale;
}

bool sampleElevation() {
  updateSettledFromInt2();

  float elev = 0;
  bool got = false;

  if (g_fifoOk) {
    uint8_t st[2] = {0, 0};
    if (readRegs(REG_FIFO_STATUS1, st, 2)) {
      uint16_t unread = (uint16_t)st[0] | ((uint16_t)(st[1] & 0x03) << 8);
      // Ab 2 Samples mitteln – niedrige Latenz, trotzdem etwas Mittelung
      if (unread >= 2 || digitalRead(PIN_INT1) == HIGH) {
        got = readElevationFromFifo(elev, false);
      }
    }
  }

  if (!got) {
    for (int attempt = 0; attempt < 8; attempt++) {
      if (readElevation(elev)) {
        got = true;
        break;
      }
      delayMicroseconds(800);
    }
  }

  if (!got) {
    return false;
  }

  // Bei Bewegung schneller folgen; im Stillstand stärker glätten
  float alpha = g_settled ? (filterAlpha * 0.35f) : (filterAlpha * 1.8f);
  if (alpha < 0.01f) {
    alpha = 0.01f;
  }
  if (alpha > 1.0f) {
    alpha = 1.0f;
  }

  if (isnan(g_elevFiltered)) {
    g_elevFiltered = elev;
  } else {
    g_elevFiltered = wrap360(g_elevFiltered + alpha * angleDiffDeg(elev, g_elevFiltered));
  }
  g_liveSeq++;
  return true;
}

void resetElevationFilter() {
  g_elevFiltered = NAN;
}

uint32_t liveSequence() {
  return g_liveSeq;
}

static bool g_calib1Ok = false;
static bool g_calib2Ok = false;
static float g_calibA = 0;
static float g_calibB = 0;

/** Mittelwert mehrerer Rohwinkel (ohne calibOffset), zirkular. */
static bool averageRawElevation(float &outDeg, int wantSamples) {
  float sumX = 0;
  float sumY = 0;
  int n = 0;
  for (int i = 0; i < wantSamples * 5 && n < wantSamples; i++) {
    float e = 0;
    if (readElevation(e, true)) {
      const float r = e * (PI / 180.0f);
      sumX += cosf(r);
      sumY += sinf(r);
      n++;
    }
    delay(4);
  }
  if (n < (wantSamples / 2)) {
    return false;
  }
  outDeg = wrap360(atan2f(sumY, sumX) * (180.0f / PI));
  return true;
}

void applyCalibOffset(float offsetDeg) {
  calibOffsetDeg = clampOffset180(offsetDeg);
  saveConfig();
  g_elevFiltered = NAN;  // Filter mit neuem Offset neu aufbauen
  sampleElevation();
}

bool calibLevelCapture1(float &outA) {
  float a = 0;
  if (!averageRawElevation(a, 25)) {
    return false;
  }
  g_calibA = a;
  g_calib1Ok = true;
  g_calib2Ok = false;
  outA = a;
  return true;
}

bool calibLevelCapture2(float &outB, float &outMid, float &outOffset, float &outSepDeg) {
  if (!g_calib1Ok) {
    return false;
  }
  float b = 0;
  if (!averageRawElevation(b, 25)) {
    return false;
  }
  g_calibB = b;
  g_calib2Ok = true;

  const float sep = angleDiffDeg(b, g_calibA);  // −180…+180
  const float mid = wrap360(g_calibA + 0.5f * sep);
  // Soll: Mittelpunkt der beiden Ebenen-Messungen = 90°
  const float offset = clampOffset180(90.0f - mid);

  outB = b;
  outMid = mid;
  outOffset = offset;
  outSepDeg = fabsf(sep);
  return true;
}

void calibLevelClear() {
  g_calib1Ok = false;
  g_calib2Ok = false;
  g_calibA = 0;
  g_calibB = 0;
}

bool calibLevelGet(float &a, bool &aOk, float &b, bool &bOk) {
  a = g_calibA;
  aOk = g_calib1Ok;
  b = g_calibB;
  bOk = g_calib2Ok;
  return true;
}

void deviceUid(char *out, size_t outLen) {
  if (out == nullptr || outLen == 0) {
    return;
  }
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  snprintf(out, outLen, "%02X%02X%02X", mac[3], mac[4], mac[5]);
}

static void debugPrintf(const char *fmt, ...) {
  if (!debugSerial) {
    return;
  }
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
}

// ---------------------------------------------------------------------------
// NVS
// ---------------------------------------------------------------------------

/** NVS nach Partitionwechsel ggf. neu initialisieren (Standard-ESP-IDF-Muster). */
static bool ensureNvsReady() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  return err == ESP_OK || err == ESP_ERR_INVALID_STATE;
}

void saveConfig() {
  if (!g_prefs.begin("incl", false)) {
    ensureNvsReady();
    if (!g_prefs.begin("incl", false)) {
      return;
    }
  }
  g_prefs.putBool("swapXY", swapXY);
  g_prefs.putBool("invRef", invertRef);
  g_prefs.putBool("invSens", invertSense);
  g_prefs.putBool("invRot", invertRotationDir);
  g_prefs.putBool("lim180", limitElevation0to180);
  g_prefs.putFloat("calib", calibOffsetDeg);
  g_prefs.putInt("mountRot", mountRotationDeg);
  g_prefs.putInt("elevDec", elevDecimals);
  g_prefs.putBool("fs1g", fullScale1g);
  g_prefs.putFloat("filt", filterAlpha);
  g_prefs.putBool("debug", debugSerial);
  g_prefs.putString("uiLang", uiLang);
  g_prefs.end();
}

static int normalizeMountRotation(int deg) {
  if (deg == 0 || deg == 90 || deg == 180 || deg == 270) {
    return deg;
  }
  return DEF_MOUNT_ROT;
}

static void normalizeUiLang() {
  if (strcmp(uiLang, "en") != 0) {
    strncpy(uiLang, "de", sizeof(uiLang));
    uiLang[sizeof(uiLang) - 1] = '\0';
  }
}

void loadConfig() {
  // Read/Write: legt Namespace an, vermeidet NOT_FOUND-Spam bei Erststart
  if (!g_prefs.begin("incl", false)) {
    ensureNvsReady();
    if (!g_prefs.begin("incl", false)) {
      return;  // Defaults behalten
    }
    g_prefs.end();
    saveConfig();
    return;
  }

  const bool hasData = g_prefs.isKey("swapXY");
  if (hasData) {
    swapXY = g_prefs.getBool("swapXY", swapXY);
    invertRef = g_prefs.getBool("invRef", invertRef);
    invertSense = g_prefs.getBool("invSens", invertSense);
    invertRotationDir = g_prefs.getBool("invRot", invertRotationDir);
    limitElevation0to180 = g_prefs.getBool("lim180", limitElevation0to180);
    calibOffsetDeg = g_prefs.getFloat("calib", calibOffsetDeg);
    mountRotationDeg = g_prefs.getInt("mountRot", mountRotationDeg);
    elevDecimals = g_prefs.getInt("elevDec", elevDecimals);
    fullScale1g = g_prefs.getBool("fs1g", fullScale1g);
    filterAlpha = g_prefs.getFloat("filt", filterAlpha);
    debugSerial = g_prefs.getBool("debug", debugSerial);
    {
      String lang = g_prefs.getString("uiLang", DEF_UI_LANG);
      strncpy(uiLang, lang.c_str(), sizeof(uiLang) - 1);
      uiLang[sizeof(uiLang) - 1] = '\0';
    }
  }
  g_prefs.end();

  if (!hasData) {
    saveConfig();
  }

  if (elevDecimals < 0) {
    elevDecimals = 0;
  }
  if (elevDecimals > 2) {
    elevDecimals = 2;
  }
  if (filterAlpha < 0.01f) {
    filterAlpha = 0.01f;
  }
  if (filterAlpha > 1.0f) {
    filterAlpha = 1.0f;
  }
  calibOffsetDeg = clampOffset180(calibOffsetDeg);
  mountRotationDeg = normalizeMountRotation(mountRotationDeg);
  normalizeUiLang();
}

void factoryResetConfig() {
  if (g_prefs.begin("incl", false)) {
    g_prefs.clear();
    g_prefs.end();
  }

  swapXY = DEF_SWAP_XY;
  invertRef = DEF_INVERT_REF;
  invertSense = DEF_INVERT_SENSE;
  invertRotationDir = DEF_INVERT_ROT;
  limitElevation0to180 = DEF_LIMIT_180;
  calibOffsetDeg = DEF_CALIB;
  mountRotationDeg = DEF_MOUNT_ROT;
  elevDecimals = DEF_ELEV_DEC;
  fullScale1g = DEF_FS_1G;
  filterAlpha = DEF_FILTER;
  debugSerial = DEF_DEBUG;
  strncpy(uiLang, DEF_UI_LANG, sizeof(uiLang) - 1);
  uiLang[sizeof(uiLang) - 1] = '\0';
  saveConfig();
}


static void blinkLbLedConfirm(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_LBLED, HIGH);
    delay(100);
    digitalWrite(PIN_LBLED, LOW);
    delay(100);
  }
}

static void toggleSoftAp() {
  if (webUiIsActive()) {
    webUiEnd();
    debugPrintf("SoftAP aus\n");
    return;
  }
  char uid[16];
  char ssid[32];
  deviceUid(uid, sizeof(uid));
  snprintf(ssid, sizeof(ssid), "INKLINO-%s", uid);
  webUiBegin(ssid, SOFTAP_PASS);
  debugPrintf("SoftAP an: %s / %s @ 192.168.4.1\n", ssid, SOFTAP_PASS);
}

/** Debounced Short-Press (Loslassen) → SoftAP Toggle */
static void pollButton() {
  bool rawHigh = digitalRead(PIN_BTN) != LOW;
  uint32_t now = millis();
  if (rawHigh != g_btnLastRaw) {
    g_btnLastRaw = rawHigh;
    g_btnLastChangeMs = now;
  }
  if ((now - g_btnLastChangeMs) < BTN_DEBOUNCE_MS) {
    return;
  }
  if (rawHigh == g_btnLastStable) {
    return;
  }
  // Flanke nach Debounce
  if (g_btnLastStable == false && rawHigh == true) {
    toggleSoftAp();
  }
  g_btnLastStable = rawHigh;
}

// ---------------------------------------------------------------------------
// RS485-Protokoll auf UART0 (GPIO21 TX / GPIO20 RX)
// ---------------------------------------------------------------------------

static void protoReply(const char *msg) {
  if (msg == nullptr) {
    return;
  }
  RS485.write(reinterpret_cast<const uint8_t *>(msg), strlen(msg));
  RS485.flush();
}

static void protoAck(const char *cmd, const char *value) {
  char out[PROTO_BUF_MAX];
  snprintf(out, sizeof(out), "#ACK_%s,%s$", cmd, value);
  protoReply(out);
}

static void protoAckInt(const char *cmd, int v) {
  char val[16];
  snprintf(val, sizeof(val), "%d", v);
  protoAck(cmd, val);
}

static void protoAckFloat(const char *cmd, float v, int decimals) {
  char val[24];
  if (decimals <= 0) {
    snprintf(val, sizeof(val), "%.0f", v);
  } else if (decimals == 1) {
    snprintf(val, sizeof(val), "%.1f", v);
  } else {
    snprintf(val, sizeof(val), "%.2f", v);
  }
  protoAck(cmd, val);
}

static void protoNack(const char *cmd) {
  char out[PROTO_BUF_MAX];
  snprintf(out, sizeof(out), "#NACK_%s,ERR$", cmd);
  protoReply(out);
}

static bool parseBool01(const char *s, bool &out) {
  if (strcmp(s, "0") == 0) {
    out = false;
    return true;
  }
  if (strcmp(s, "1") == 0) {
    out = true;
    return true;
  }
  return false;
}

static void handleFrame(char *frame) {
  // frame ohne # und $ , z.B. "GETINVSENS" oder "SETINVSENS,1"
  char *comma = strchr(frame, ',');
  char *value = nullptr;
  if (comma) {
    *comma = '\0';
    value = comma + 1;
  }

  const bool isGet = (strncmp(frame, "GET", 3) == 0);
  const bool isSet = (strncmp(frame, "SET", 3) == 0);
  if (!isGet && !isSet) {
    protoNack(frame);
    return;
  }

  const char *name = frame + 3;  // nach GET/SET

  // --- GETDG (Winkel) ---
  if (isGet && strcmp(name, "DG") == 0) {
    protoAckFloat("GETDG", currentElevationOut(), elevDecimals);
    return;
  }

  // --- GETSETTLED (INT2 sleep/stationary) ---
  if (isGet && strcmp(name, "SETTLED") == 0) {
    protoAckInt("GETSETTLED", isSettled() ? 1 : 0);
    return;
  }

  // --- Bool-Config Helper ---
  auto handleBool = [&](const char *key, bool &var) {
    char cmd[32];
    if (isGet) {
      snprintf(cmd, sizeof(cmd), "GET%s", key);
      protoAckInt(cmd, var ? 1 : 0);
      return true;
    }
    snprintf(cmd, sizeof(cmd), "SET%s", key);
    if (!value) {
      protoNack(cmd);
      return true;
    }
    bool v = false;
    if (!parseBool01(value, v)) {
      protoNack(cmd);
      return true;
    }
    var = v;
    saveConfig();
    protoAckInt(cmd, var ? 1 : 0);
    return true;
  };

  if (strcmp(name, "SWAPXY") == 0) {
    handleBool("SWAPXY", swapXY);
    return;
  }
  if (strcmp(name, "INVREF") == 0) {
    handleBool("INVREF", invertRef);
    return;
  }
  if (strcmp(name, "INVSENS") == 0) {
    handleBool("INVSENS", invertSense);
    return;
  }
  if (strcmp(name, "INVROT") == 0) {
    handleBool("INVROT", invertRotationDir);
    return;
  }
  if (strcmp(name, "MOUNTROT") == 0) {
    if (isGet) {
      protoAckInt("GETMOUNTROT", mountRotationDeg);
      return;
    }
    if (!value) {
      protoNack("SETMOUNTROT");
      return;
    }
    char *end = nullptr;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') {
      protoNack("SETMOUNTROT");
      return;
    }
    const int n = normalizeMountRotation((int)v);
    if (n != (int)v) {
      protoNack("SETMOUNTROT");
      return;
    }
    mountRotationDeg = n;
    g_elevFiltered = NAN;
    saveConfig();
    protoAckInt("SETMOUNTROT", mountRotationDeg);
    return;
  }
  if (strcmp(name, "LIMIT180") == 0) {
    handleBool("LIMIT180", limitElevation0to180);
    return;
  }
  if (strcmp(name, "FS1G") == 0) {
    if (isGet) {
      protoAckInt("GETFS1G", fullScale1g ? 1 : 0);
      return;
    }
    if (!value) {
      protoNack("SETFS1G");
      return;
    }
    bool v = false;
    if (!parseBool01(value, v)) {
      protoNack("SETFS1G");
      return;
    }
    fullScale1g = v;
    applyFullScale();
    saveConfig();
    protoAckInt("SETFS1G", fullScale1g ? 1 : 0);
    return;
  }
  if (strcmp(name, "DEBUG") == 0) {
    if (isGet) {
      protoAckInt("GETDEBUG", debugSerial ? 1 : 0);
      return;
    }
    if (!value) {
      protoNack("SETDEBUG");
      return;
    }
    bool v = false;
    if (!parseBool01(value, v)) {
      protoNack("SETDEBUG");
      return;
    }
    debugSerial = v;
    saveConfig();
    Serial.printf("USB-Winkeldebug: %s\n", debugSerial ? "EIN" : "AUS");
    protoAckInt("SETDEBUG", debugSerial ? 1 : 0);
    return;
  }

  if (strcmp(name, "CALIB") == 0) {
    if (isGet) {
      protoAckFloat("GETCALIB", calibOffsetDeg, 2);
      return;
    }
    if (!value) {
      protoNack("SETCALIB");
      return;
    }
    char *end = nullptr;
    float v = strtof(value, &end);
    if (end == value || *end != '\0') {
      protoNack("SETCALIB");
      return;
    }
    if (v < -180.0f || v > 180.0f) {
      protoNack("SETCALIB");
      return;
    }
    calibOffsetDeg = v;
    saveConfig();
    protoAckFloat("SETCALIB", calibOffsetDeg, 2);
    return;
  }

  if (strcmp(name, "ELEVDEC") == 0) {
    if (isGet) {
      protoAckInt("GETELEVDEC", elevDecimals);
      return;
    }
    if (!value) {
      protoNack("SETELEVDEC");
      return;
    }
    char *end = nullptr;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0' || v < 0 || v > 2) {
      protoNack("SETELEVDEC");
      return;
    }
    elevDecimals = (int)v;
    saveConfig();
    protoAckInt("SETELEVDEC", elevDecimals);
    return;
  }

  if (strcmp(name, "FILTER") == 0) {
    if (isGet) {
      protoAckFloat("GETFILTER", filterAlpha, 2);
      return;
    }
    if (!value) {
      protoNack("SETFILTER");
      return;
    }
    char *end = nullptr;
    float v = strtof(value, &end);
    if (end == value || *end != '\0' || v < 0.01f || v > 1.0f) {
      protoNack("SETFILTER");
      return;
    }
    filterAlpha = v;
    saveConfig();
    protoAckFloat("SETFILTER", filterAlpha, 2);
    return;
  }

  protoNack(frame);
}

static void pollProtocol() {
  while (RS485.available()) {
    char c = (char)RS485.read();
    if (c == '#') {
      g_rxActive = true;
      g_rxLen = 0;
      continue;
    }
    if (!g_rxActive) {
      continue;
    }
    if (c == '$') {
      g_rxBuf[g_rxLen] = '\0';
      g_rxActive = false;
      if (g_rxLen > 0) {
        handleFrame(g_rxBuf);
      }
      g_rxLen = 0;
      continue;
    }
    // Steuerzeichen ignorieren
    if (c == '\r' || c == '\n' || c == '\0') {
      continue;
    }
    if (g_rxLen + 1 >= PROTO_BUF_MAX) {
      g_rxActive = false;
      g_rxLen = 0;
      continue;
    }
    g_rxBuf[g_rxLen++] = c;
  }
}

/**
 * LBLED:
 * - Fehler: dauerhaft an
 * - SoftAP aktiv: zwei kurze Blitze, dann 250 ms Pause
 * - normal: 500 ms an / 500 ms aus
 */
static void updateLbLed() {
  if (g_errorActive) {
    digitalWrite(PIN_LBLED, HIGH);
    return;
  }
  if (webUiIsActive()) {
    // 70 ms an, 70 ms aus, 70 ms an, 250 ms Pause → Zyklus 460 ms
    constexpr uint32_t BLINK_ON = 70;
    constexpr uint32_t BLINK_GAP = 70;
    constexpr uint32_t PAUSE_MS = 250;
    constexpr uint32_t CYCLE =
        BLINK_ON + BLINK_GAP + BLINK_ON + PAUSE_MS;  // 460
    const uint32_t t = millis() % CYCLE;
    const bool on =
        (t < BLINK_ON) ||
        (t >= (BLINK_ON + BLINK_GAP) && t < (BLINK_ON + BLINK_GAP + BLINK_ON));
    digitalWrite(PIN_LBLED, on ? HIGH : LOW);
    return;
  }
  const uint32_t t = millis() % 1000;
  digitalWrite(PIN_LBLED, (t < 500) ? HIGH : LOW);
}

// ---------------------------------------------------------------------------

void setup() {
  pinMode(PIN_LBLED, OUTPUT);
  digitalWrite(PIN_LBLED, HIGH);  // Start: an bis Init ok

  // GPIO0: externer Pull-up, LOW = gedrückt → Werkreset
  pinMode(PIN_BTN, INPUT);

  // USB-CDC (ESP32-C3): Boot-Banner immer ausgeben
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(300);
  Serial.println();
  Serial.println(F("Inklinometer IIS2ICLX / ESP32-C3 boot"));

  bool nvsOk = ensureNvsReady();
  Serial.printf("NVS init: %s\n", nvsOk ? "ok" : "fail");

  const int btn = digitalRead(PIN_BTN);
  Serial.printf("GPIO0=%s\n", btn == LOW ? "LOW (factory)" : "HIGH");
  if (btn == LOW) {
    Serial.println(F("Factory reset…"));
    Serial.flush();
    factoryResetConfig();
    blinkLbLedConfirm(3);
    ESP.restart();
  }
  g_btnLastRaw = true;
  g_btnLastStable = true;
  g_btnLastChangeMs = millis();

  // RS485 UART0: RX=20, TX=21
  RS485.begin(115200, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);

  loadConfig();
  Serial.println(F("Config geladen"));

  // INT1/INT2: kein Pull-up (Datenblatt)
  pinMode(PIN_INT1, INPUT);
  pinMode(PIN_INT2, INPUT);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(200);

  while (!initSensor()) {
    g_errorActive = true;
    digitalWrite(PIN_LBLED, HIGH);
    Serial.println(F("Sensor-Init fehlgeschlagen – Retry"));
    delay(2000);
  }
  g_errorActive = false;
  updateSettledFromInt2();

  char uid[16];
  deviceUid(uid, sizeof(uid));
  Serial.printf("Ready UID=%s RS485 TX%d RX%d INT1=IO%d INT2=IO%d LBLED=IO%d fifo=%d\n",
                uid, PIN_RS485_TX, PIN_RS485_RX, PIN_INT1, PIN_INT2, PIN_LBLED,
                g_fifoOk ? 1 : 0);
}

void loop() {
  if (fullScale1g != g_appliedFs1g) {
    if (!applyFullScale()) {
      g_errorActive = true;
    }
  }

  sampleElevation();

  pollProtocol();
  pollButton();
  webUiLoop();

  if (webUiIsActive()) {
    // Unter SoftAP erneut sampeln (I2C/WiFi-Interferenz)
    sampleElevation();
    static uint32_t lastPush = 0;
    if (millis() - lastPush >= 100) {
      lastPush = millis();
      webUiPushLive();
    }
  }

  if (debugSerial) {
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 100) {
      lastPrint = millis();
      int dec = elevDecimals;
      if (dec < 0) {
        dec = 0;
      }
      if (dec > 2) {
        dec = 2;
      }
      // Direkt auf USB-CDC, damit Ausgabe sicher sichtbar ist
      Serial.printf("Elev: %.*f deg settled=%d\n", dec, (double)currentElevationOut(),
                    isSettled() ? 1 : 0);
    }
  }

  // Config-Mode: weiter blinken (kein Fehler)
  updateLbLed();
  if (webUiIsActive()) {
    yield();
  } else {
    delay(2);
  }
}
