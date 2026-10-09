# Inklinometer DK8DE IIS2ICLX – Bedienungs- und Einrichtungsanleitung

**Firmware:** siehe [`src/version.h`](../src/version.h) bzw. Anzeige in der Web-UI  
**Sprache:** Deutsch · English: [`Manual_EN.md`](Manual_EN.md)

Dieses Dokument beschreibt den **fertigen Sensor** (Gehäuse/Platine mit Spannungsversorgung und RS485), die Inbetriebnahme, das Flashen der Firmware, die SoftAP-Weboberfläche und das RS485-Textprotokoll.

---

## 1. Lieferumfang und Überblick

Das Modul misst die **Antennen-Elevation** mit einem ST **IIS2ICLX** (hochauflösender 2-Achsen-Beschleunigungssensor) und einem **ESP32-C3**. Die Winkelausgabe und die Konfiguration erfolgen typischerweise über **RS485**. Zusätzlich kann ein **WLAN-Zugangspunkt (SoftAP)** mit Web-UI für Live-Anzeige, Einstellungen und Firmware-Update genutzt werden.

| Funktion | Kurzbeschreibung |
|----------|------------------|
| Elevation | Gefilterter Winkel in Grad (0…360° bzw. begrenzt 0…180°) |
| RS485 | Textprotokoll `#GET…$` / `#SET…$` bei 115200 8N1 |
| SoftAP | Konfiguration, Live-Winkel, OTA-Update |
| Mechanik-Badge | Erkennung von Rucken/Schlägen (nicht langsamer Drehung) |

---

## 2. Anschlüsse am Gerät

### 2.1 Spannungsversorgung

| Anschluss | Spezifikation |
|-----------|---------------|
| Gleichspannung | **7 … 36 V DC** |
| Polarität | laut Beschriftung auf der Platine / Steckerbelegung beachten |

**Hinweise**

- Spannungsbereich nicht unter- oder überschreiten.
- Vor dem Anklemmen Polarität prüfen.
- Nach dem Einschalten startet die Firmware selbstständig; die Lebens-LED (LBLED) beginnt zu blinken (siehe Abschnitt 3).

### 2.2 RS485

| Anschluss | Bedeutung |
|-----------|-----------|
| **A** | RS485-Datenleitung A (nicht invertierend, je nach Wandler-Beschriftung) |
| **B** | RS485-Datenleitung B (invertierend) |

**Bus-Parameter (Protokollseite)**

| Parameter | Wert |
|-----------|------|
| Baudrate | **115200** |
| Datenbits | 8 |
| Parität | keine (N) |
| Stoppbits | 1 |
| Flusskontrolle | keine |
| Abschluss | je nach Buslänge/Topologie ggf. 120 Ω am Busende |

Die RS485-Schnittstelle am Modul arbeitet mit **automatischer DE/RE-Umschaltung** (kein separater Richtungspin nötig).

### 2.3 LEDs

#### RS485 TX- und RX-LED

Am Gerät sind zwei LEDs für den RS485-Datenverkehr vorgesehen:

| LED | Bedeutung |
|-----|-----------|
| **TX** | Leuchtet/blinkt kurz, wenn das Modul Daten **sendet** (Antworten auf dem Bus) |
| **RX** | Leuchtet/blinkt kurz, wenn das Modul Daten **empfängt** (Befehle auf dem Bus) |

Damit lässt sich prüfen, ob die Gegenstelle überhaupt Frames ankommt und ob das Modul antwortet.

#### LBLED (Status / Lebenszeichen)

Die **LBLED** zeigt den Betriebszustand der Firmware (nicht den RS485-Verkehr):

| Zustand | LED-Muster |
|---------|------------|
| Normalbetrieb | **500 ms an / 500 ms aus** (gleichmäßiges Blinken) |
| SoftAP / WLAN-Config aktiv | **zwei kurze Blitze**, danach **ca. 250 ms Pause**, dann wiederholen |
| Fehler (z. B. Sensor nicht erreichbar) | **dauerhaft an** |
| Werkreset beim Boot bestätigt | **3× schnell blinken**, danach Neustart |

---

## 3. Taster (Reset / SoftAP)

Am Gerät befindet sich ein **Taster** (intern mit GPIO0 verbunden, gegen GND, mit Pull-up). Er hat **zwei verschiedene Funktionen**, je nachdem *wann* er betätigt wird:

### 3.1 Zur Laufzeit – SoftAP (WLAN) ein-/ausschalten

1. Gerät ist bereits gestartet (LBLED blinkt normal).
2. Taster **kurz drücken und loslassen**.
3. SoftAP wird **umgeschaltet**:
   - war aus → SoftAP **ein** (LBLED wechselt auf Doppelblink-Muster)
   - war an → SoftAP **aus** (LBLED wieder Normalblinken)

**Zusätzlich schaltet SoftAP automatisch aus**, wenn der letzte WLAN-Client getrennt wurde und ca. **2,5 s** lang kein Client mehr verbunden ist.

### 3.2 Beim Einschalten – Werkseinstellung (Factory Reset)

1. Gerät **spannungslos** machen bzw. Neustart vorbereiten.
2. Taster **gedrückt halten**.
3. Spannung anlegen bzw. Reset auslösen (so dass der Taster beim Boot noch LOW ist).
4. Firmware erkennt „Factory“: NVS-Konfiguration wird gelöscht, Compile-Defaults werden geschrieben.
5. LBLED bestätigt mit **3× schnellem Blinken**, danach Neustart mit Werkseinstellungen.

**Wichtig:** Das ist ein **Konfigurations-Reset** (Winkel-Offset, Filter, Stoßschwelle, …), kein reines „MCU Hard-Reset“ ohne Löschen. Gespeicherte Einstellungen gehen dabei verloren.

---

## 4. Montage des Sensors

Für eine eindeutige Elevation über den vollen Bereich:

- Sensor **senkrecht** in der Elevationsebene montieren (beide Messachsen X/Y in der Kippebene).
- Scharnier / Drehachse möglichst senkrecht zur Sensorfläche.
- Full-Scale **±1 g** (Default) oder **±2 g** verwenden; ±0,5 g ist für diese Montage ungeeignet (Sättigung).

Nach der Montage typischerweise:

1. SoftAP öffnen oder RS485 nutzen.
2. Montage-Drehung (`MOUNTROT`) und Invertierungen bei Bedarf setzen.
3. Optional 2-Punkt-Ebenkalibrierung in der Web-UI durchführen.

---

## 5. Firmware flashen

Es gibt drei übliche Wege.

### 5.1 Webflasher (empfohlen für Erstinstallation)

1. Chromium-Browser (Chrome/Edge) öffnen.
2. Seite öffnen:  
   **https://dk8de.github.io/Inklinometer_IIS2ICLXTR/**
3. Gerät per **USB** (ESP32-C3 native USB) verbinden.
4. **Installieren** / Connect wählen und Anweisungen im Dialog folgen.
5. Es wird `factory.bin` geschrieben (Bootloader + Partitionstabelle + App ab Adresse `0x0`).

Downloads auf derselben Seite:

| Datei | Verwendung |
|-------|------------|
| `factory.bin` | Komplettflash / Erstinstallation / Recovery |
| `firmware.bin` | nur App – für SoftAP-OTA (Web-Tab **Update**) |

### 5.2 PlatformIO (Entwicklung)

Im Projektverzeichnis:

```bash
# Linux / macOS / Git Bash
./build.sh                 # Version syncen, bauen, hochladen
./build.sh --no-upload
UPLOAD_PORT=COM34 ./build.sh

# Windows PowerShell
.\build.ps1
.\build.ps1 -NoUpload
.\build.ps1 -UploadPort COM34
```

Oder manuell:

```bash
pio run -e esp32-c3 -t upload
pio device monitor -b 115200
```

Nach Änderung der Partitionstabelle einmalig:

```bash
pio run -e esp32-c3 -t erase -t upload
```

**Hinweis USB-C3:** Monitor während des Uploads schließen. Bei Boot-Loop ggf. BOOT halten → Reset → BOOT loslassen.

### 5.3 OTA über SoftAP (nur App-Update)

1. SoftAP per Taster einschalten.
2. Mit dem WLAN `INKLINO-<UID>` verbinden (Passwort siehe unten).
3. Im Browser `http://192.168.4.1` öffnen, anmelden.
4. Tab **Update** → `firmware.bin` hochladen.
5. Nach Erfolg startet das Gerät neu.

---

## 6. SoftAP und Web-UI

### 6.1 Verbinden

| | |
|--|--|
| SoftAP starten | Taster kurz drücken (LBLED → Doppelblink) |
| SSID | `INKLINO-<UID>` (UID = letzte 3 Bytes der MAC, hex) |
| WLAN-Passwort | `Rotorconfig` |
| IP der Web-UI | `http://192.168.4.1` |
| Login | Benutzer `admin` / Passwort `Rotorconfig` |

Das RS485-Protokoll läuft **parallel** weiter, solange SoftAP aktiv ist.

### 6.2 Tabs

| Tab | Inhalt |
|-----|--------|
| **Status** | Live-Elevation, Badge **stabil / Ruck**, Systeminfos |
| **Konfiguration** | Montage, Filter, Kalibrier-Offset, Stoßschwelle, Full-Scale, Debug, … |
| **Kalibrierung** | 2-Punkt-Ebenkalibrierung |
| **Update** | OTA mit `firmware.bin` |
| **Reset** | Neustart / Werkseinstellung |

Sprache der Web-UI: Deutsch / Englisch (wird in NVS gespeichert).

### 6.3 Stoßschwelle und Badge „stabil / Ruck“

Der Badge nutzt INT2 (Wake-up des Sensors):

- **stabil** = keine Beschleunigungsänderung über der Schwelle
- **Ruck** = Stoß / mechanische Unruhe erkannt

Das ist **kein** Indikator für langsame Winkelbewegung. Die Empfindlichkeit stellt man mit **Stoßschwelle** (`1…63`, Default **25**) ein: kleiner = empfindlicher.

---

## 7. RS485-Protokoll – Grundlagen

### 7.1 Frame-Format

- Jeder Befehl beginnt mit `#` und endet mit `$`.
- **Keine** Zeilenumbrüche (`\r` / `\n`) innerhalb des Frames.
- Zeichen dazwischen: Befehl und optionaler Wert.

| Typ | Format | Beispiel |
|-----|--------|----------|
| Lesen | `#GET<NAME>$` | `#GETDG$` |
| Schreiben | `#SET<NAME>,<Wert>$` | `#SETFILTER,0.35$` |
| Erfolg | `#ACK_<CMD>,<Wert>$` | `#ACK_GETDG,45.12$` |
| Fehler | `#NACK_<CMD>,ERR$` | `#NACK_SETFILTER,ERR$` |

Bei SET steht in der ACK-Antwort der **übernommene** Wert (nach Clamp/Normalisierung, sofern gültig).

### 7.2 Typische Sitzung

```text
#GETDG$
#ACK_GETDG,45.12$

#GETSHOCK$
#ACK_GETSHOCK,0$

#SETSHOCKTHS,25$
#ACK_SETSHOCKTHS,25$

#SETCALIB,-1.50$
#ACK_SETCALIB,-1.50$
```

Ungültiger Wert:

```text
#SETFILTER,2$
#NACK_SETFILTER,ERR$
```

### 7.3 Speicherung (NVS)

Jeder **erfolgreiche SET**-Befehl speichert die Konfiguration im Flash (NVS-Namespace `incl`). Die Werte bleiben nach Spannungsausfall erhalten. Gleiches gilt für Speichern in der Web-UI.

---

## 8. Befehlsreferenz (ausführlich)

### 8.1 Messung und Mechanik

#### `GETDG` – Elevation

- **Format:** `#GETDG$`
- **Antwort:** `#ACK_GETDG,<winkel>$`
- **Bedeutung:** Aktueller gefilterter Elevationswinkel in Grad.
- **Nachkommastellen:** abhängig von `ELEVDEC` (0 / 1 / 2).
- **Kein SET** – nur Lesen.

#### `GETSETTLED` – Mechanik stabil?

- **Format:** `#GETSETTLED$`
- **Antwort:** `#ACK_GETSETTLED,0$` oder `,1$`
- **`1`** = stabil (kein Stoß über Schwelle)
- **`0`** = Ruck/Stoß erkannt
- Entspricht dem Web-Badge „stabil“, wenn `1`.

#### `GETSHOCK` – Stoß erkannt? (invertierte Lesart)

- **Format:** `#GETSHOCK$`
- **`1`** = Ruck/Stoß
- **`0`** = stabil
- Praktisch: `GETSHOCK = 1 - GETSETTLED`.

#### `GETSHOCKTHS` / `SETSHOCKTHS` – Stoßschwelle

- **Lesen:** `#GETSHOCKTHS$`
- **Schreiben:** `#SETSHOCKTHS,<1…63>$`
- **Default:** `25`
- **Wirkung:** Register `WAKE_UP_THS` am IIS2ICLX.  
  1 LSB ≈ Full-Scale / 64 (bei ±1 g also feiner als bei ±2 g).
- **Sofort aktiv** am Sensor; zusätzlich NVS-Speicherung.
- Kleiner Wert → empfindlicher; größer → nur härtere Schläge.

---

### 8.2 Winkel-Darstellung und Filter

#### `GETELEVDEC` / `SETELEVDEC` – Nachkommastellen

- Werte: `0`, `1` oder `2`
- Betrifft die Formatierung von `GETDG` (und die Web-Anzeige).

#### `GETFILTER` / `SETFILTER` – EMA-Filter α

- Bereich: `0.01` … `1.0`
- Größer = Winkel folgt schneller (weniger Glättung).
- Intern zusätzlich skaliert:
  - bei **stabil**: α × 0,35 (stärker geglättet)
  - bei **Ruck**: α × 1,8 (schneller)
- Default: `0.15`

#### `GETCALIB` / `SETCALIB` – Kalibrier-Offset

- Bereich: −180.0 … +180.0 Grad
- Wird auf den berechneten Winkel addiert/angewendet (Offset-Korrektur).
- Default: `0.0`

#### `GETLIMIT180` / `SETLIMIT180` – Soft-Limit 0…180°

- `0` = voller Kreis (0…360°-Logik der Firmware-Ausgabe je nach Pipeline)
- `1` = Ausgabe auf **0…180°** begrenzen:
  - 0…180 → unverändert
  - >180 … ≤270 → **180**
  - >270 … <360 → **0**

#### `GETMOUNTROT` / `SETMOUNTROT` – Montage-Drehung

- Erlaubt: `0`, `90`, `180`, `270`
- Dreht die logische 0°-Seite der Montage.
- Beim Ändern wird der Elevationsfilter neu gestartet.

#### `GETINVROT` / `SETINVROT` – Drehrichtung umkehren

- `0` / `1`
- `1` = Winkel wächst gegen den Uhrzeigersinn (je nach Montagekonvention der Firmware).

#### `GETSWAPXY` / `SETSWAPXY` – Achsen tauschen

- `0` / `1` – X/Y des Sensors vertauschen (Montageanpassung).

#### `GETINVREF` / `SETINVREF` – Referenz invertieren

- `0` / `1` – Invertierung der Referenzachse.

#### `GETINVSENS` / `SETINVSENS` – Sense invertieren

- `0` / `1` – Invertierung der Sense-Richtung.

#### `GETFS1G` / `SETFS1G` – Full-Scale

- `1` = ±1 g (Default, höhere Auflösung)
- `0` = ±2 g
- Wird **sofort** in den Sensor geschrieben.
- Beeinflusst auch die reale mg-Stufe der Stoßschwelle (LSB ≈ FS/64).

---

### 8.3 Diagnose

#### `GETDEBUG` / `SETDEBUG` – USB-Debug

- `1` = Elevation ca. 10×/s auf USB-CDC (115200 Baud) ausgeben
- `0` = aus
- Unabhängig vom RS485-Protokoll; hilfreich bei USB-Monitor.

---

### 8.4 Übersichtstabelle

| Bedeutung | GET | SET | Werte |
|-----------|-----|-----|-------|
| Elevation | `GETDG` | — | Grad |
| Stabil (kein Stoß) | `GETSETTLED` | — | 0 / 1 |
| Stoß erkannt | `GETSHOCK` | — | 0 / 1 |
| Stoßschwelle | `GETSHOCKTHS` | `SETSHOCKTHS` | 1…63 (Default 25) |
| Achsen tauschen | `GETSWAPXY` | `SETSWAPXY` | 0 / 1 |
| REF invertieren | `GETINVREF` | `SETINVREF` | 0 / 1 |
| SENSE invertieren | `GETINVSENS` | `SETINVSENS` | 0 / 1 |
| Drehrichtung umkehren | `GETINVROT` | `SETINVROT` | 0 / 1 |
| Montage-Drehung | `GETMOUNTROT` | `SETMOUNTROT` | 0 / 90 / 180 / 270 |
| Limit 0…180° | `GETLIMIT180` | `SETLIMIT180` | 0 / 1 |
| Kalibrier-Offset | `GETCALIB` | `SETCALIB` | −180.0…+180.0 |
| Full-Scale ±1 g | `GETFS1G` | `SETFS1G` | 0 / 1 |
| Nachkommastellen | `GETELEVDEC` | `SETELEVDEC` | 0 / 1 / 2 |
| Filter α | `GETFILTER` | `SETFILTER` | 0.01…1.0 |
| USB-Debug | `GETDEBUG` | `SETDEBUG` | 0 / 1 |

---

## 9. Erste Inbetriebnahme (Checkliste)

1. **Spannung** 7–36 V DC anlegen (Polarität prüfen).
2. LBLED: normales Blinken = OK; dauerhaft an = Sensorfehler prüfen.
3. **RS485 A/B** mit Steuerung verbinden (Baud 115200 8N1).
4. Test: `#GETDG$` senden → RX-LED kurz aktiv, dann TX-LED und Antwort `#ACK_GETDG,…$`.
5. Optional SoftAP: Taster kurz → WLAN `INKLINO-…` → `http://192.168.4.1` → Login.
6. Montage/Offset/Filter/Stoßschwelle einstellen und speichern (SET oder Web **Speichern**).
7. Optional: 2-Punkt-Kalibrierung im Web-Tab **Kalibrierung**.

---

## 10. 2-Punkt-Ebenkalibrierung (Web)

1. Sensor auf ebene Fläche legen → **1. Messung**.
2. Auf der Stelle horizontal ca. **180°** drehen → **2. Messung**.
3. Beide Rohwinkel liegen typisch nahe beieinander (~90°).
4. Offset ≈ `90° − Mittelwert` → **Offset übernehmen** (NVS).

Die 180°-Drehung mittelt kleine Asymmetrien der Auflage.

---

## 11. Werkseinstellungen (Compile-Defaults)

Nach Factory-Reset (Taster beim Boot oder Web-Tab Reset):

| Parameter | Default |
|-----------|---------|
| `SWAPXY` | 1 |
| `INVREF` | 0 |
| `INVSENS` | 1 |
| `INVROT` | 1 |
| `LIMIT180` | 0 |
| `CALIB` | 0.0 |
| `MOUNTROT` | 0 |
| `ELEVDEC` | 2 |
| `FS1G` | 1 (±1 g) |
| `FILTER` | 0.15 |
| `SHOCKTHS` | 25 |
| `DEBUG` | 0 |
| UI-Sprache | de |

---

## 12. Fehlerhilfe

| Beobachtung | Mögliche Ursache / Maßnahme |
|-------------|-----------------------------|
| LBLED dauerhaft an | Sensor-I2C-Fehler; Verkabelung/Sensor prüfen, neu starten |
| Keine RS485-Antwort | A/B vertauscht, Baudrate, gemeinsame Masse, Abschluss |
| RX-LED blinkt, keine TX-LED | Frame ungültig (fehlendes `#`/`$`, Zeilenumbruch) |
| SoftAP erscheint nicht | Taster kurz drücken; LBLED auf Doppelblink prüfen |
| SoftAP verschwindet von allein | Normal: ca. 2,5 s nach Trennung des letzten Clients |
| Badge bleibt „stabil“ bei langsamer Drehung | Erwartet – nur Stoß/Ruck, Schwelle ggf. verkleinern |
| Winkel nach Power-Cycle „falsch“ | Montage/Kalibrierung/`MOUNTROT` prüfen |
| Upload per USB scheitert | Monitor schließen; ggf. BOOT-Modus |

---

## 13. Weiterführende Links

- Entwickler-README: [`../README.md`](../README.md)
- English manual: [`Manual_EN.md`](Manual_EN.md)
- Webflasher: https://dk8de.github.io/Inklinometer_IIS2ICLXTR/
