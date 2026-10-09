#pragma once

#include <Arduino.h>

// Laufzeit-Konfiguration (NVS) – in main.cpp definiert
extern bool swapXY;
extern bool invertRef;
extern bool invertSense;
extern bool invertRotationDir;
extern bool limitElevation0to180;
extern float calibOffsetDeg;
extern int mountRotationDeg;  // 0 / 90 / 180 / 270
extern int elevDecimals;
extern bool fullScale1g;
extern float filterAlpha;
extern bool debugSerial;
extern bool g_errorActive;
extern char uiLang[4];  // "de" oder "en"

void saveConfig();
void loadConfig();
void factoryResetConfig();
bool applyFullScale();
float currentElevationOut();
/** Sensor lesen + Filter aktualisieren (mit Retry). */
bool sampleElevation();
void resetElevationFilter();
uint32_t liveSequence();
/** INT2 sleep/stationary: true = Stillstand (eingelaufen). */
bool isSettled();
void deviceUid(char *out, size_t outLen);  // z.B. "C3A1B2"

/** 2-Punkt-Ebenkalibrierung (Rohwinkel ohne Offset). */
bool calibLevelCapture1(float &outA);
bool calibLevelCapture2(float &outB, float &outMid, float &outOffset, float &outSepDeg);
void calibLevelClear();
bool calibLevelGet(float &a, bool &aOk, float &b, bool &bOk);
/** Offset setzen, speichern, Filter neu starten. */
void applyCalibOffset(float offsetDeg);
