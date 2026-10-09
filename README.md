# Inklinometer IIS2ICLX – ESP32-C3

Hochauflösende Antennen-Elevation über ST IIS2ICLX, Abfrage und Konfiguration per RS485-Textprotokoll. Zusätzlich SoftAP-Web-UI mit Live-Winkel, Konfiguration und Dual-OTA. Einstellungen werden im NVS gespeichert.

## Hardware

| Signal | ESP32-C3 |
|--------|----------|
| SDA | GPIO4 |
| SCL | GPIO5 |
| INT1 (FIFO Watermark) | GPIO6 |
| INT2 (Sleep/Stationary) | GPIO7 |
| RS485 TX | GPIO21 (UART0) |
| RS485 RX | GPIO20 (UART0) |
| LBLED (Lebensblink) | GPIO10 |
| Taster | GPIO0 gegen GND (externer Pull-up) |
| USB | nativer USB-CDC (optionaler Debug) |

- I2C: 400 kHz / Sensor je nach SA0 (Firmware: `0x6B`)
- RS485-Wandler mit **automatischer** DE/RE-Umschaltung (kein DE-Pin nötig)
- Baudrate Protokoll: **115200 8N1**

### LBLED (GPIO10)

| Zustand | LED |
|---------|-----|
| Loop läuft normal | **500 ms an / 500 ms aus** |
| SoftAP / Config-Mode aktiv | **2× kurz blinken**, dann **250 ms Pause** (wiederholt) |
| Fehler (z. B. Sensor-Init / Full-Scale schreiben) | **dauerhaft an** |
| Werkreset beim Boot bestätigt | **3× schnell blinken**, dann Neustart |

### Taster (GPIO0)

| Zeitpunkt | Verhalten |
|-----------|-----------|
| Beim Boot gedrückt (LOW) | NVS-Namespace `incl` löschen, Compile-Defaults speichern, Neustart |
| Zur Laufzeit Short-Press | SoftAP + Web-UI **ein/aus** (Toggle, bleibt bis Neustart oder zweitem Druck) |

## SoftAP / Web-UI

| | |
|--|--|
| SSID | `INKLINO-<UID>` (UID = letzte 3 MAC-Bytes) |
| Passwort | `Rotorconfig` |
| IP | `192.168.4.1` |
| Web-Login | `admin` / `Rotorconfig` |

Tabs: **Status** (Live-Winkel), **Konfiguration** (NVS-Felder), **Update** (OTA `firmware.bin`), **Reset** (Neustart / Werkseinstellung).

Das RS485-Protokoll läuft parallel weiter, solange SoftAP aktiv ist.

## Serielle Schnittstellen

| Port | Verwendung |
|------|------------|
| UART0 (GPIO21/20) | RS485-Protokoll (`#GET…$` / `#SET…$`) |
| USB-CDC | Debug-Ausgabe, wenn „Debug über USB“ ein (`#SETDEBUG,1$` oder Web-Schalter) |

## Protokoll

### Frame-Format

- Jeder Befehl beginnt mit `#` und endet mit `$`
- **Keine** Zeilenumbrüche (`\r`/`\n`) im Frame
- GET: `#GET<NAME>$`
- SET: `#SET<NAME>,<Wert>$`
- Erfolg: `#ACK_<CMD>,<Wert>$`
- Fehler: `#NACK_<CMD>,ERR$`

### Beispiele

```
#GETDG$
#ACK_GETDG,45.12$

#SETINVSENS,1$
#ACK_SETINVSENS,1$

#GETINVSENS$
#ACK_GETINVSENS,1$

#SETCALIB,-2.50$
#ACK_SETCALIB,-2.50$

#SETFILTER,0.35$
#ACK_SETFILTER,0.35$

#SETDEBUG,1$
#ACK_SETDEBUG,1$
```

Ungültiger Wert:

```
#SETFILTER,2$
#NACK_SETFILTER,ERR$
```

### Befehlsübersicht

| Bedeutung | GET | SET | Wertebereich |
|-----------|-----|-----|--------------|
| Elevation (gefiltert) | `GETDG` | — | Winkel in °, Nachkommastellen laut `ELEVDEC` |
| Stillstand (INT2 Sleep/Stationary) | `GETSETTLED` | — | `0` = bewegt, `1` = ruhig / eingelaufen |
| Achsen tauschen (swapXY) | `GETSWAPXY` | `SETSWAPXY` | `0` / `1` |
| REF invertieren | `GETINVREF` | `SETINVREF` | `0` / `1` |
| SENSE invertieren | `GETINVSENS` | `SETINVSENS` | `0` / `1` |
| Drehrichtung umkehren | `GETINVROT` | `SETINVROT` | `0` / `1` (1 = gegen Uhrzeigersinn vergrößert) |
| Montage-Drehung (0°-Seite) | `GETMOUNTROT` | `SETMOUNTROT` | `0` / `90` / `180` / `270` |
| Winkel auf 0…180° begrenzen | `GETLIMIT180` | `SETLIMIT180` | `0` / `1` |
| Kalibrier-Offset | `GETCALIB` | `SETCALIB` | −180.0 … +180.0 |
| Full-Scale ±1 g | `GETFS1G` | `SETFS1G` | `0` = ±2 g, `1` = ±1 g |
| Anzeige-Nachkommastellen | `GETELEVDEC` | `SETELEVDEC` | `0` / `1` / `2` |
| EMA-Filter alpha | `GETFILTER` | `SETFILTER` | 0.01 … 1.0 (größer = schneller; bei Settled ×0,25) |
| USB-Debug | `GETDEBUG` | `SETDEBUG` | `0` / `1` |

### Rauschfilter (INT1 / INT2)

- **INT1 (GPIO6):** FIFO-Watermark (8 Samples @ 104 Hz) → Mittelwert der neuesten `ax`/`ay`, dann `atan2`
- **Sensor-LPF2:** Bandbreite ODR/20 (~5 Hz) – Rauschen runter, ohne Sekunden-Nachlauf
- **INT2 (GPIO7):** Sleep/Stationary-Status (Pegel) → `GETSETTLED` / Web-Badge; Settled → stärkeres EMA, Moving → schnelleres EMA
- Kein Pull-up an INT1/INT2 (Datenblatt)

### Soft-Limit (`LIMIT180`)

Wenn aktiv (`1`):

- Rohwinkel 0…180 → unverändert
- >180 … ≤270 → Ausgabe **180**
- >270 … <360 → Ausgabe **0**

### NVS

Jeder erfolgreiche **SET**-Befehl (und Web-Config-Speichern) speichert die Konfiguration im Flash (Namespace `incl`). Nach Reset bleiben die Werte erhalten.

### 2-Punkt-Ebenkalibrierung (Web)

1. Sensor auf ebene Fläche → **1. Messung**
2. Auf der Stelle horizontal ca. **180°** drehen → **2. Messung**
3. Beide Rohwinkel sind meist **nahe beieinander** (~90°) → Mittelwert → `offset = 90° − Mittelwert` → **Übernehmen**

Die 180°-Drehung mittelt kleine Asymmetrien der Auflage. Ein Abstand von ~180° wäre nur bei einem Flip in der Messebene typisch (andere Geometrie).

## OTA

- Dual-OTA-Partitionen (`partitions.csv`: `ota_0` / `ota_1`, je ~1,88 MB)
- Web-Tab **Update**: `.pio/build/esp32-c3/firmware.bin` hochladen
- Nach erfolgreichem Flash: automatischer Neustart

## Montage (Elevation)

Sensor **senkrecht** in der Elevationsebene montieren (beide Achsen X/Y in der Kippebene). Scharnier senkrecht zur Sensorfläche. Dann ist `atan2` über 0…360° eindeutig (auch nach Power-Cycle).

**Hinweis:** Full-Scale ±0.5 g ist für diese Montage ungeeignet (Sättigung bei ca. 45°/135°). Nutze ±1 g oder ±2 g.

## Webflasher (GitHub Actions)

Bei Push auf `main` baut die Action `.github/workflows/build-webflasher.yml`:

| Datei | Verwendung |
|-------|------------|
| `factory.bin` | Komplettflash ab Adresse `0x0` (Bootloader + Partitionstabelle + App) |
| `firmware.bin` | nur App – SoftAP-Web-Tab **Update** (OTA) |

Keine separate Datenpartition → zwei Images reichen. Artifact heißt `Inklinometer-<Version>` und enthält genau diese beiden `.bin` (+ kurze `README.txt`).

- Webflasher (ESP Web Tools) nutzt `factory.bin`
- Pages: https://dk8de.github.io/Inklinometer_IIS2ICLXTR/

## Build / Flash

```bash
pio run -t upload
pio device monitor
```

Nach Partitionstabellen-Wechsel einmalig:

```bash
pio run -t erase -t upload
```

**ESP32-C3 USB (OTG):** Upload nutzt `board_upload.before_reset = usb_reset` (kein PROG/RESET nötig), sofern der Chip über die native USB-Serial/JTAG-Schnittstelle hängt und die Firmware noch antwortet. Monitor während Upload schließen (`monitor_dtr/rts = 0` verhindert Reset-Loops im Monitor). Bei Boot-Loop oder fehlendem USB-JTAG weiterhin manuell: BOOT halten → Reset → BOOT loslassen.

USB-Monitor zeigt Elevation nur bei `#SETDEBUG,1$`. Protokoll-Tests über RS485 an GPIO21/20.
