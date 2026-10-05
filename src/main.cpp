/**
 * IIS2ICLX – Antennen-Elevation + RS485 GET/SET-Protokoll
 * ESP32-C3 | SDA=GPIO4 | SCL=GPIO5
 * RS485: UART0 TX=GPIO21 RX=GPIO20 @ 115200
 * USB-CDC: optionaler Debug (SETDEBUG)
 */

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static constexpr int PIN_SDA = 4;
static constexpr int PIN_SCL = 5;
static constexpr int PIN_RS485_RX = 20;
static constexpr int PIN_RS485_TX = 21;
static constexpr uint8_t ADDR = 0x6B;

static constexpr uint8_t REG_WHO_AM_I = 0x0F;
static constexpr uint8_t REG_CTRL1_XL = 0x10;
static constexpr uint8_t REG_CTRL3_C  = 0x12;
static constexpr uint8_t REG_CTRL9_XL = 0x18;
static constexpr uint8_t REG_STATUS   = 0x1E;
static constexpr uint8_t REG_OUTX_L_A = 0x28;

static constexpr uint8_t WHO_AM_I_VAL = 0x6B;
static constexpr uint8_t CTRL1_XL_FS_2G = 0x4C;
static constexpr uint8_t CTRL1_XL_FS_1G = 0x48;
static constexpr float SENS_2G_MG_LSB = 0.061f;
static constexpr float SENS_1G_MG_LSB = 0.031f;

static constexpr size_t PROTO_BUF_MAX = 96;

// --- Laufzeit-Konfiguration (NVS) ---
bool swapXY = true;
bool invertRef = false;
bool invertSense = true;
bool invertRotationDir = true;
bool limitElevation0to180 = false;
float calibOffsetDeg = 0.0f;
int elevDecimals = 2;
bool fullScale1g = true;
float filterAlpha = 0.15f;  // 0.01…1.0; größer = schneller
bool debugSerial = false;   // USB-CDC Debug

static float g_elevFiltered = NAN;
static bool g_appliedFs1g = false;
static Preferences g_prefs;

static char g_rxBuf[PROTO_BUF_MAX];
static size_t g_rxLen = 0;
static bool g_rxActive = false;

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

static bool applyFullScale() {
  if (!writeReg(REG_CTRL1_XL, ctrl1XlForFs())) {
    return false;
  }
  g_appliedFs1g = fullScale1g;
  delay(20);
  return true;
}

static bool initSensor() {
  uint8_t who = 0;
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
  if (!writeReg(REG_CTRL3_C, 0x44)) {
    return false;
  }
  if (!applyFullScale()) {
    return false;
  }
  delay(50);
  return true;
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

  if (!raw) {
    elev = wrap360(elev + clampOffset180(calibOffsetDeg));
  }

  elevationDeg = elev;
  return true;
}

static float currentElevationOut() {
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

static void saveConfig() {
  g_prefs.begin("incl", false);
  g_prefs.putBool("swapXY", swapXY);
  g_prefs.putBool("invRef", invertRef);
  g_prefs.putBool("invSens", invertSense);
  g_prefs.putBool("invRot", invertRotationDir);
  g_prefs.putBool("lim180", limitElevation0to180);
  g_prefs.putFloat("calib", calibOffsetDeg);
  g_prefs.putInt("elevDec", elevDecimals);
  g_prefs.putBool("fs1g", fullScale1g);
  g_prefs.putFloat("filt", filterAlpha);
  g_prefs.putBool("debug", debugSerial);
  g_prefs.end();
}

static void loadConfig() {
  g_prefs.begin("incl", true);
  swapXY = g_prefs.getBool("swapXY", swapXY);
  invertRef = g_prefs.getBool("invRef", invertRef);
  invertSense = g_prefs.getBool("invSens", invertSense);
  invertRotationDir = g_prefs.getBool("invRot", invertRotationDir);
  limitElevation0to180 = g_prefs.getBool("lim180", limitElevation0to180);
  calibOffsetDeg = g_prefs.getFloat("calib", calibOffsetDeg);
  elevDecimals = g_prefs.getInt("elevDec", elevDecimals);
  fullScale1g = g_prefs.getBool("fs1g", fullScale1g);
  filterAlpha = g_prefs.getFloat("filt", filterAlpha);
  debugSerial = g_prefs.getBool("debug", debugSerial);
  g_prefs.end();

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
}

// ---------------------------------------------------------------------------
// RS485-Protokoll auf Serial0
// ---------------------------------------------------------------------------

static void protoReply(const char *msg) {
  Serial0.print(msg);
  Serial0.flush();
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
    handleBool("DEBUG", debugSerial);
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
  while (Serial0.available()) {
    char c = (char)Serial0.read();
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

// ---------------------------------------------------------------------------

void setup() {
  // USB-CDC Debug
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(300);

  // RS485 UART0: RX=20, TX=21
  Serial0.begin(115200, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);

  loadConfig();

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  while (!initSensor()) {
    debugPrintf("Sensor-Init fehlgeschlagen\n");
    delay(2000);
  }

  debugPrintf("RS485 UART0 TX%d RX%d | USB-Debug=%d\n",
              PIN_RS485_TX, PIN_RS485_RX, debugSerial ? 1 : 0);
}

void loop() {
  if (fullScale1g != g_appliedFs1g) {
    applyFullScale();
  }

  pollProtocol();

  float elev = 0;
  if (readElevation(elev)) {
    if (isnan(g_elevFiltered)) {
      g_elevFiltered = elev;
    } else {
      g_elevFiltered =
          wrap360(g_elevFiltered + filterAlpha * angleDiffDeg(elev, g_elevFiltered));
    }
  }

  if (debugSerial) {
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 50) {
      lastPrint = millis();
      float out = currentElevationOut();
      int dec = elevDecimals;
      if (dec < 0) {
        dec = 0;
      }
      if (dec > 2) {
        dec = 2;
      }
      if (dec == 0) {
        debugPrintf("Elevation: %3.0f deg\n", out);
      } else if (dec == 1) {
        debugPrintf("Elevation: %5.1f deg\n", out);
      } else {
        debugPrintf("Elevation: %6.2f deg\n", out);
      }
    }
  }

  delay(5);
}
