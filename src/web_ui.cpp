#include "web_ui.h"

#include <Update.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <string.h>

#include "app_api.h"
#include "version.h"
#include "web_content.h"

static WebServer *g_server = nullptr;
static WebSocketsServer g_ws(81);
static bool g_webActive = false;
static bool g_wsStarted = false;
static bool g_apHadClient = false;
static uint32_t g_apEmptySinceMs = 0;
static constexpr uint32_t AP_IDLE_SHUTDOWN_MS = 2500;

static const char *WEB_USER = "admin";
static const char *WEB_PASS = "Rotorconfig";
static const char *FW_VERSION = FW_VERSION_STR;

static bool checkAuth() {
  if (g_server == nullptr) {
    return false;
  }
  if (g_server->authenticate(WEB_USER, WEB_PASS)) {
    return true;
  }
  g_server->requestAuthentication();
  return false;
}

static void sendJson(int code, const String &body) {
  if (g_server == nullptr) {
    return;
  }
  g_server->sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  g_server->sendHeader("Pragma", "no-cache");
  g_server->sendHeader("Connection", "close");
  g_server->send(code, "application/json", body);
}

static void handleRoot() {
  if (!checkAuth()) {
    return;
  }
  g_server->sendHeader("Connection", "close");
  g_server->sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  g_server->sendHeader("Pragma", "no-cache");
  g_server->sendHeader("Expires", "0");
  g_server->send_P(200, "text/html", WEB_PAGE);
}

static void handleStatus() {
  if (!checkAuth()) {
    return;
  }
  char uid[16];
  deviceUid(uid, sizeof(uid));
  String ssid = WiFi.softAPSSID();
  String ip = WiFi.softAPIP().toString();
  char buf[448];
  snprintf(buf, sizeof(buf),
           "{\"uptime\":%lu,\"error\":%s,\"ssid\":\"%s\",\"ip\":\"%s\","
           "\"uid\":\"%s\",\"fw\":\"%s\",\"heap\":%u,"
           "\"elevation\":%.4f,\"decimals\":%d,\"debug\":%s,\"settled\":%s}",
           (unsigned long)(millis() / 1000UL), g_errorActive ? "true" : "false",
           ssid.c_str(), ip.c_str(), uid, FW_VERSION, (unsigned)ESP.getFreeHeap(),
           (double)currentElevationOut(), elevDecimals,
           debugSerial ? "true" : "false", isSettled() ? "true" : "false");
  sendJson(200, buf);
}

static void fillLiveJson(char *buf, size_t buflen) {
  // Frisches Sample + Systemfelder (damit Status nach Reload sofort voll ist)
  sampleElevation();
  char uid[16];
  deviceUid(uid, sizeof(uid));
  String ssid = WiFi.softAPSSID();
  String ip = WiFi.softAPIP().toString();
  snprintf(buf, buflen,
           "{\"elevation\":%.4f,\"decimals\":%d,\"error\":%s,\"ms\":%lu,"
           "\"heap\":%u,\"settled\":%s,\"uptime\":%lu,"
           "\"ssid\":\"%s\",\"ip\":\"%s\",\"uid\":\"%s\",\"fw\":\"%s\"}",
           (double)currentElevationOut(), elevDecimals,
           g_errorActive ? "true" : "false", (unsigned long)millis(),
           (unsigned)ESP.getFreeHeap(),
           isSettled() ? "true" : "false", (unsigned long)(millis() / 1000UL),
           ssid.c_str(), ip.c_str(), uid, FW_VERSION);
}

static void handleAngle() {
  char buf[384];
  fillLiveJson(buf, sizeof(buf));
  sendJson(200, buf);
}

/** Live-Poll: Winkel + Systeminfos für Statuskarte. */
static void handleLive() {
  char buf[384];
  fillLiveJson(buf, sizeof(buf));
  sendJson(200, buf);
}

static void handleGetConfig() {
  if (!checkAuth()) {
    return;
  }
  char buf[420];
  snprintf(buf, sizeof(buf),
           "{\"invertRotationDir\":%s,\"limit180\":%s,\"fullScale1g\":%s,"
           "\"debug\":%s,\"calib\":%.3f,\"mountRotation\":%d,\"filter\":%.3f,"
           "\"decimals\":%d,\"lang\":\"%s\"}",
           invertRotationDir ? "true" : "false",
           limitElevation0to180 ? "true" : "false", fullScale1g ? "true" : "false",
           debugSerial ? "true" : "false", (double)calibOffsetDeg, mountRotationDeg,
           (double)filterAlpha, elevDecimals, uiLang);
  sendJson(200, buf);
}

static bool jsonBool(const String &body, const char *key, bool &out) {
  String needle = String("\"") + key + "\":";
  int i = body.indexOf(needle);
  if (i < 0) {
    return false;
  }
  i += needle.length();
  while (i < (int)body.length() && (body[i] == ' ' || body[i] == '\t')) {
    i++;
  }
  if (body.startsWith("true", i)) {
    out = true;
    return true;
  }
  if (body.startsWith("false", i)) {
    out = false;
    return true;
  }
  return false;
}

static bool jsonNumber(const String &body, const char *key, float &out) {
  String needle = String("\"") + key + "\":";
  int i = body.indexOf(needle);
  if (i < 0) {
    return false;
  }
  i += needle.length();
  while (i < (int)body.length() && (body[i] == ' ' || body[i] == '\t')) {
    i++;
  }
  char *end = nullptr;
  float v = strtof(body.c_str() + i, &end);
  if (end == body.c_str() + i) {
    return false;
  }
  out = v;
  return true;
}

static void handlePostConfig() {
  if (!checkAuth()) {
    return;
  }
  String body = g_server->arg("plain");
  if (body.length() == 0) {
    sendJson(400, "{\"ok\":false,\"err\":\"empty\"}");
    return;
  }

  bool b;
  float f;
  if (jsonBool(body, "invertRotationDir", b)) {
    invertRotationDir = b;
  }
  if (jsonBool(body, "limit180", b)) {
    limitElevation0to180 = b;
  }
  if (jsonBool(body, "fullScale1g", b)) {
    fullScale1g = b;
  }
  if (jsonBool(body, "debug", b)) {
    debugSerial = b;
  }
  if (jsonNumber(body, "calib", f)) {
    if (f < -180.0f) {
      f = -180.0f;
    }
    if (f > 180.0f) {
      f = 180.0f;
    }
    calibOffsetDeg = f;
  }
  if (jsonNumber(body, "mountRotation", f)) {
    const int m = (int)lroundf(f);
    if (m != 0 && m != 90 && m != 180 && m != 270) {
      sendJson(400, "{\"ok\":false,\"err\":\"mountRotation\"}");
      return;
    }
    if (m != mountRotationDeg) {
      mountRotationDeg = m;
      resetElevationFilter();
    }
  }
  if (jsonNumber(body, "filter", f)) {
    if (f < 0.01f || f > 1.0f) {
      sendJson(400, "{\"ok\":false,\"err\":\"filter\"}");
      return;
    }
    filterAlpha = f;
  }
  if (jsonNumber(body, "decimals", f)) {
    int d = (int)lroundf(f);
    if (d < 0 || d > 2) {
      sendJson(400, "{\"ok\":false,\"err\":\"decimals\"}");
      return;
    }
    elevDecimals = d;
  }
  {
    String needle = "\"lang\":\"";
    int i = body.indexOf(needle);
    if (i >= 0) {
      i += needle.length();
      if (body.startsWith("en\"", i)) {
        strncpy(uiLang, "en", sizeof(uiLang) - 1);
        uiLang[sizeof(uiLang) - 1] = '\0';
      } else if (body.startsWith("de\"", i)) {
        strncpy(uiLang, "de", sizeof(uiLang) - 1);
        uiLang[sizeof(uiLang) - 1] = '\0';
      }
    }
  }

  if (!applyFullScale()) {
    sendJson(500, "{\"ok\":false,\"err\":\"fs\"}");
    return;
  }
  saveConfig();
  sendJson(200, "{\"ok\":true}");
}

static void handleGetLang() {
  if (!checkAuth()) {
    return;
  }
  char buf[48];
  snprintf(buf, sizeof(buf), "{\"lang\":\"%s\"}", uiLang);
  sendJson(200, buf);
}

static void handlePostLang() {
  if (!checkAuth()) {
    return;
  }
  String body = g_server->arg("plain");
  String needle = "\"lang\":\"";
  int i = body.indexOf(needle);
  if (i < 0) {
    sendJson(400, "{\"ok\":false,\"err\":\"lang\"}");
    return;
  }
  i += needle.length();
  if (body.startsWith("en\"", i)) {
    strncpy(uiLang, "en", sizeof(uiLang) - 1);
  } else if (body.startsWith("de\"", i)) {
    strncpy(uiLang, "de", sizeof(uiLang) - 1);
  } else {
    sendJson(400, "{\"ok\":false,\"err\":\"lang\"}");
    return;
  }
  uiLang[sizeof(uiLang) - 1] = '\0';
  saveConfig();
  char buf[48];
  snprintf(buf, sizeof(buf), "{\"ok\":true,\"lang\":\"%s\"}", uiLang);
  sendJson(200, buf);
}

static void handleUpdateInfo() {
  if (!checkAuth()) {
    return;
  }
  const esp_partition_t *running = esp_ota_get_running_partition();
  char buf[192];
  snprintf(buf, sizeof(buf),
           "{\"fw\":\"%s\",\"partition\":\"%s\",\"size\":%u}", FW_VERSION,
           running ? running->label : "?", running ? (unsigned)running->size : 0U);
  sendJson(200, buf);
}

static void handleUpdateUpload() {
  if (g_server == nullptr) {
    return;
  }
  HTTPUpload &upload = g_server->upload();
  if (upload.status == UPLOAD_FILE_START) {
    size_t maxSketch = ESP.getFreeSketchSpace();
    if (!Update.begin(maxSketch)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) {
      Update.printError(Serial);
    }
  }
}

static void handleUpdateDone() {
  if (!checkAuth()) {
    return;
  }
  if (Update.hasError()) {
    sendJson(500, "{\"ok\":false}");
    return;
  }
  sendJson(200, "{\"ok\":true}");
}

static void handleReboot() {
  if (!checkAuth()) {
    return;
  }
  sendJson(200, "{\"ok\":true}");
  delay(200);
  ESP.restart();
}

static void handleFactory() {
  if (!checkAuth()) {
    return;
  }
  factoryResetConfig();
  sendJson(200, "{\"ok\":true}");
  delay(200);
  ESP.restart();
}

static void handleGetDebug() {
  if (!checkAuth()) {
    return;
  }
  char buf[48];
  snprintf(buf, sizeof(buf), "{\"debug\":%s}", debugSerial ? "true" : "false");
  sendJson(200, buf);
}

static void handlePostDebug() {
  if (!checkAuth()) {
    return;
  }
  String body = g_server->arg("plain");
  bool on = debugSerial;
  if (jsonBool(body, "debug", on) || jsonBool(body, "enabled", on)) {
    const bool was = debugSerial;
    debugSerial = on;
    saveConfig();
    if (was != debugSerial) {
      Serial.printf("USB-Winkeldebug: %s\n", debugSerial ? "EIN" : "AUS");
    }
    sendJson(200, debugSerial ? "{\"ok\":true,\"debug\":true}"
                              : "{\"ok\":true,\"debug\":false}");
    return;
  }
  sendJson(400, "{\"ok\":false,\"err\":\"debug\"}");
}

static void handleCalibGet() {
  if (!checkAuth()) {
    return;
  }
  float a = 0;
  float b = 0;
  bool aOk = false;
  bool bOk = false;
  calibLevelGet(a, aOk, b, bOk);
  char buf[192];
  snprintf(buf, sizeof(buf),
           "{\"a\":%.3f,\"aOk\":%s,\"b\":%.3f,\"bOk\":%s,\"calib\":%.3f}",
           (double)a, aOk ? "true" : "false", (double)b, bOk ? "true" : "false",
           (double)calibOffsetDeg);
  sendJson(200, buf);
}

static void handleCalibCapture1() {
  if (!checkAuth()) {
    return;
  }
  float a = 0;
  if (!calibLevelCapture1(a)) {
    sendJson(500, "{\"ok\":false,\"err\":\"sample\"}");
    return;
  }
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"ok\":true,\"a\":%.3f}", (double)a);
  sendJson(200, buf);
}

static void handleCalibCapture2() {
  if (!checkAuth()) {
    return;
  }
  float b = 0;
  float mid = 0;
  float offset = 0;
  float sep = 0;
  if (!calibLevelCapture2(b, mid, offset, sep)) {
    sendJson(500, "{\"ok\":false,\"err\":\"sample\"}");
    return;
  }
  char buf[280];
  // mode: "flat" = beide Messungen ~gleich (Drehung auf der Stelle), "flip" = ~180° Abstand
  const char *mode = (sep < 45.0f) ? "flat" : ((sep >= 150.0f) ? "flip" : "odd");
  snprintf(buf, sizeof(buf),
           "{\"ok\":true,\"b\":%.3f,\"mid\":%.3f,\"offset\":%.3f,\"sep\":%.2f,"
           "\"mode\":\"%s\",\"near180\":%s}",
           (double)b, (double)mid, (double)offset, (double)sep, mode,
           (sep >= 150.0f) ? "true" : "false");
  sendJson(200, buf);
}

static void handleCalibApply() {
  if (!checkAuth()) {
    return;
  }
  String body = g_server->arg("plain");
  float offset = 0;
  if (!jsonNumber(body, "offset", offset)) {
    sendJson(400, "{\"ok\":false,\"err\":\"offset\"}");
    return;
  }
  applyCalibOffset(offset);
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"ok\":true,\"calib\":%.3f}", (double)calibOffsetDeg);
  sendJson(200, buf);
}

static void handleCalibClear() {
  if (!checkAuth()) {
    return;
  }
  calibLevelClear();
  sendJson(200, "{\"ok\":true}");
}

/** Handy-Captive-Portal-Probes schnell beantworten (sonst laufen TCP-Slots voll). */
static void handleCaptiveOk() {
  if (g_server == nullptr) {
    return;
  }
  g_server->sendHeader("Connection", "close");
  g_server->sendHeader("Cache-Control", "no-store");
  g_server->send(204, "text/plain", "");
}

static void handleCaptiveHtml() {
  if (g_server == nullptr) {
    return;
  }
  g_server->sendHeader("Connection", "close");
  g_server->sendHeader("Cache-Control", "no-store");
  g_server->send(200, "text/html",
                 "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
}

static void handleNotFound() {
  if (g_server == nullptr) {
    return;
  }
  g_server->sendHeader("Connection", "close");
  g_server->send(404, "text/plain", "Not Found");
}

static void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  (void)payload;
  (void)length;
  if (type == WStype_CONNECTED) {
    char buf[384];
    fillLiveJson(buf, sizeof(buf));
    g_ws.sendTXT(num, buf);
  }
}

void webUiBegin(const char *ssid, const char *password) {
  if (g_webActive) {
    return;
  }
  if (g_server == nullptr) {
    g_server = new WebServer(80);
  }

  WiFi.mode(WIFI_AP);
  // SoftAP: kein Modem-Sleep (sonst hakelige / abreißende HTTP-Antworten)
  WiFi.setSleep(false);
  // Etwas weniger TX-Leistung → weniger I2C-Störungen auf engem Board
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  // Kanal 1, max. 2 Stationen (weniger TCP-Last)
  WiFi.softAP(ssid, password, 1, 0, 2);

  g_server->on("/", HTTP_GET, handleRoot);
  g_server->on("/api/status", HTTP_GET, handleStatus);
  g_server->on("/api/live", HTTP_GET, handleLive);
  g_server->on("/api/angle", HTTP_GET, handleAngle);
  g_server->on("/api/config", HTTP_GET, handleGetConfig);
  g_server->on("/api/config", HTTP_POST, handlePostConfig);
  g_server->on("/api/update/info", HTTP_GET, handleUpdateInfo);
  g_server->on(
      "/api/update", HTTP_POST, handleUpdateDone,
      []() { handleUpdateUpload(); });
  g_server->on("/api/reboot", HTTP_POST, handleReboot);
  g_server->on("/api/factory", HTTP_POST, handleFactory);
  g_server->on("/api/debug", HTTP_GET, handleGetDebug);
  g_server->on("/api/debug", HTTP_POST, handlePostDebug);
  g_server->on("/api/lang", HTTP_GET, handleGetLang);
  g_server->on("/api/lang", HTTP_POST, handlePostLang);
  g_server->on("/api/calib", HTTP_GET, handleCalibGet);
  g_server->on("/api/calib/1", HTTP_POST, handleCalibCapture1);
  g_server->on("/api/calib/2", HTTP_POST, handleCalibCapture2);
  g_server->on("/api/calib/apply", HTTP_POST, handleCalibApply);
  g_server->on("/api/calib/clear", HTTP_POST, handleCalibClear);

  // Captive-Portal-Probes (Android/iOS/Windows/…)
  g_server->on("/generate_204", HTTP_GET, handleCaptiveOk);
  g_server->on("/gen_204", HTTP_GET, handleCaptiveOk);
  g_server->on("/hotspot-detect.html", HTTP_GET, handleCaptiveHtml);
  g_server->on("/library/test/success.html", HTTP_GET, handleCaptiveHtml);
  g_server->on("/connecttest.txt", HTTP_GET, handleCaptiveOk);
  g_server->on("/ncsi.txt", HTTP_GET, handleCaptiveOk);
  g_server->on("/fwlink", HTTP_GET, handleCaptiveOk);
  g_server->on("/canonical.html", HTTP_GET, handleCaptiveHtml);
  g_server->onNotFound(handleNotFound);

  g_server->begin();

  g_ws.begin();
  g_ws.onEvent(onWsEvent);
  g_wsStarted = true;

  g_apHadClient = false;
  g_apEmptySinceMs = 0;
  g_webActive = true;
}

void webUiEnd() {
  if (!g_webActive) {
    return;
  }
  if (g_wsStarted) {
    g_ws.close();
    g_wsStarted = false;
  }
  if (g_server != nullptr) {
    g_server->stop();
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  g_apHadClient = false;
  g_apEmptySinceMs = 0;
  g_webActive = false;
}

void webUiLoop() {
  if (!g_webActive) {
    return;
  }
  if (g_server != nullptr) {
    for (int i = 0; i < 4; i++) {
      g_server->handleClient();
    }
  }
  if (g_wsStarted) {
    g_ws.loop();
  }

  // Nach Trennung aller SoftAP-Clients AP wieder aus (bis nächster Taster)
  const int stations = WiFi.softAPgetStationNum();
  if (stations > 0) {
    g_apHadClient = true;
    g_apEmptySinceMs = 0;
  } else if (g_apHadClient) {
    if (g_apEmptySinceMs == 0) {
      g_apEmptySinceMs = millis();
    } else if ((uint32_t)(millis() - g_apEmptySinceMs) >= AP_IDLE_SHUTDOWN_MS) {
      webUiEnd();
    }
  }
}

void webUiPushLive() {
  if (!g_webActive || !g_wsStarted) {
    return;
  }
  char buf[384];
  fillLiveJson(buf, sizeof(buf));
  // Explizit an jeden Client (zuverlässiger als broadcast auf SoftAP)
  for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
    if (g_ws.clientIsConnected(i)) {
      g_ws.sendTXT(i, buf);
    }
  }
}

bool webUiIsActive() {
  return g_webActive;
}
