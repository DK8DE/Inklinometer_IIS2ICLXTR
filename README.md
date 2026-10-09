# Inklinometer DK8DE IIS2ICLX â€“ ESP32-C3

**Firmware:** <!--FW_VERSION-->1.3.1<!--/FW_VERSION--> (einzige Quelle: [`src/version.h`](src/version.h))

HochauflÃ¶sende Antennen-Elevation Ã¼ber ST IIS2ICLX, Abfrage und Konfiguration per RS485-Textprotokoll. ZusÃ¤tzlich SoftAP-Web-UI mit Live-Winkel, Konfiguration und Dual-OTA. Einstellungen werden im NVS gespeichert.

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
- RS485-Wandler mit **automatischer** DE/RE-Umschaltung (kein DE-Pin nÃ¶tig)
- Baudrate Protokoll: **115200 8N1**

### LBLED (GPIO10)

| Zustand | LED |
|---------|-----|
| Loop lÃ¤uft normal | **500 ms an / 500 ms aus** |
| SoftAP / Config-Mode aktiv | **2Ã— kurz blinken**, dann **250 ms Pause** (wiederholt) |
| Fehler (z. B. Sensor-Init / Full-Scale schreiben) | **dauerhaft an** |
| Werkreset beim Boot bestÃ¤tigt | **3Ã— schnell blinken**, dann Neustart |

### Taster (GPIO0)

| Zeitpunkt | Verhalten |
|-----------|-----------|
| Beim Boot gedrÃ¼ckt (LOW) | NVS-Namespace `incl` lÃ¶schen, Compile-Defaults speichern, Neustart |
| Zur Laufzeit Short-Press | SoftAP + Web-UI **ein/aus** (Toggle, bleibt bis Neustart oder zweitem Druck) |

## SoftAP / Web-UI

| | |
|--|--|
| SSID | `INKLINO-<UID>` (UID = letzte 3 MAC-Bytes) |
| Passwort | `Rotorconfig` |
| IP | `192.168.4.1` |
| Web-Login | `admin` / `Rotorconfig` |

Tabs: **Status** (Live-Winkel), **Konfiguration** (NVS-Felder), **Update** (OTA `firmware.bin`), **Reset** (Neustart / Werkseinstellung).

Das RS485-Protokoll lÃ¤uft parallel weiter, solange SoftAP aktiv ist.

## Serielle Schnittstellen

| Port | Verwendung |
|------|------------|
| UART0 (GPIO21/20) | RS485-Protokoll (`#GETâ€¦$` / `#SETâ€¦$`) |
| USB-CDC | Debug-Ausgabe, wenn â€žDebug Ã¼ber USBâ€œ ein (`#SETDEBUG,1$` oder Web-Schalter) |

## Protokoll

### Frame-Format

- Jeder Befehl beginnt mit `#` und endet mit `$`
- **Keine** ZeilenumbrÃ¼che (`\r`/`\n`) im Frame
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

UngÃ¼ltiger Wert:

```
#SETFILTER,2$
#NACK_SETFILTER,ERR$
```

### BefehlsÃ¼bersicht

| Bedeutung | GET | SET | Wertebereich |
|-----------|-----|-----|--------------|
| Elevation (gefiltert) | `GETDG` | â€” | Winkel in Â°, Nachkommastellen laut `ELEVDEC` |
| Stillstand (INT2 Sleep/Stationary) | `GETSETTLED` | â€” | `0` = bewegt, `1` = ruhig / eingelaufen |
| Achsen tauschen (swapXY) | `GETSWAPXY` | `SETSWAPXY` | `0` / `1` |
| REF invertieren | `GETINVREF` | `SETINVREF` | `0` / `1` |
| SENSE invertieren | `GETINVSENS` | `SETINVSENS` | `0` / `1` |
| Drehrichtung umkehren | `GETINVROT` | `SETINVROT` | `0` / `1` (1 = gegen Uhrzeigersinn vergrÃ¶ÃŸert) |
| Montage-Drehung (0Â°-Seite) | `GETMOUNTROT` | `SETMOUNTROT` | `0` / `90` / `180` / `270` |
| Winkel auf 0â€¦180Â° begrenzen | `GETLIMIT180` | `SETLIMIT180` | `0` / `1` |
| Kalibrier-Offset | `GETCALIB` | `SETCALIB` | âˆ’180.0 â€¦ +180.0 |
| Full-Scale Â±1 g | `GETFS1G` | `SETFS1G` | `0` = Â±2 g, `1` = Â±1 g |
| Anzeige-Nachkommastellen | `GETELEVDEC` | `SETELEVDEC` | `0` / `1` / `2` |
| EMA-Filter alpha | `GETFILTER` | `SETFILTER` | 0.01 â€¦ 1.0 (grÃ¶ÃŸer = schneller; bei Settled Ã—0,25) |
| USB-Debug | `GETDEBUG` | `SETDEBUG` | `0` / `1` |

### Rauschfilter (INT1 / INT2)

- **INT1 (GPIO6):** FIFO-Watermark (8 Samples @ 104 Hz) â†’ Mittelwert der neuesten `ax`/`ay`, dann `atan2`
- **Sensor-LPF2:** Bandbreite ODR/20 (~5 Hz) â€“ Rauschen runter, ohne Sekunden-Nachlauf
- **INT2 (GPIO7):** Sleep/Stationary-Status (Pegel) â†’ `GETSETTLED` / Web-Badge; Settled â†’ stÃ¤rkeres EMA, Moving â†’ schnelleres EMA
- Kein Pull-up an INT1/INT2 (Datenblatt)

### Soft-Limit (`LIMIT180`)

Wenn aktiv (`1`):

- Rohwinkel 0â€¦180 â†’ unverÃ¤ndert
- >180 â€¦ â‰¤270 â†’ Ausgabe **180**
- >270 â€¦ <360 â†’ Ausgabe **0**

### NVS

Jeder erfolgreiche **SET**-Befehl (und Web-Config-Speichern) speichert die Konfiguration im Flash (Namespace `incl`). Nach Reset bleiben die Werte erhalten.

### 2-Punkt-Ebenkalibrierung (Web)

1. Sensor auf ebene FlÃ¤che â†’ **1. Messung**
2. Auf der Stelle horizontal ca. **180Â°** drehen â†’ **2. Messung**
3. Beide Rohwinkel sind meist **nahe beieinander** (~90Â°) â†’ Mittelwert â†’ `offset = 90Â° âˆ’ Mittelwert` â†’ **Ãœbernehmen**

Die 180Â°-Drehung mittelt kleine Asymmetrien der Auflage. Ein Abstand von ~180Â° wÃ¤re nur bei einem Flip in der Messebene typisch (andere Geometrie).

## OTA

- Dual-OTA-Partitionen (`partitions.csv`: `ota_0` / `ota_1`, je ~1,88 MB)
- Web-Tab **Update**: `.pio/build/esp32-c3/firmware.bin` hochladen
- Nach erfolgreichem Flash: automatischer Neustart

## Montage (Elevation)

Sensor **senkrecht** in der Elevationsebene montieren (beide Achsen X/Y in der Kippebene). Scharnier senkrecht zur SensorflÃ¤che. Dann ist `atan2` Ã¼ber 0â€¦360Â° eindeutig (auch nach Power-Cycle).

**Hinweis:** Full-Scale Â±0.5 g ist fÃ¼r diese Montage ungeeignet (SÃ¤ttigung bei ca. 45Â°/135Â°). Nutze Â±1 g oder Â±2 g.

## Webflasher (GitHub Actions)

Bei Push auf `main` baut die Action `.github/workflows/build-webflasher.yml`:

| Datei | Verwendung |
|-------|------------|
| `factory.bin` | Komplettflash ab Adresse `0x0` (Bootloader + Partitionstabelle + App) |
| `firmware.bin` | nur App â€“ SoftAP-Web-Tab **Update** (OTA) |

Keine separate Datenpartition â†’ zwei Images reichen. Artifact heiÃŸt `Inklinometer-<Version>` und enthÃ¤lt genau diese beiden `.bin` (+ kurze `README.txt`).

- Webflasher (ESP Web Tools) nutzt `factory.bin`
- Pages: https://dk8de.github.io/Inklinometer_IIS2ICLXTR/

## Version & Build

Version nur in [`src/version.h`](src/version.h) Ã¤ndern (`FW_VERSION_STR` / Major/Minor/Patch).

```bash
# Git Bash / Linux / macOS
./build.sh                 # sync README â†’ compile â†’ Upload auf MCU
./build.sh --no-upload
./build.sh --sync-only
UPLOAD_PORT=COM34 ./build.sh

# Windows PowerShell
.\build.ps1
.\build.ps1 -NoUpload
.\build.ps1 -SyncOnly
.\build.ps1 -UploadPort COM34
```

GitHub Actions: bei jedem Push **Artifacts** (`factory.bin` / `firmware.bin`). Ein **GitHub Release** (`vX.Y.Z`) entsteht nur, wenn diese Version noch kein Release-Tag hat.

## Build / Flash (manuell)

```bash
pio run -t upload
pio device monitor
```

Nach Partitionstabellen-Wechsel einmalig:

```bash
pio run -t erase -t upload
```

**ESP32-C3 USB (OTG):** Upload nutzt `board_upload.before_reset = usb_reset` (kein PROG/RESET nÃ¶tig), sofern der Chip Ã¼ber die native USB-Serial/JTAG-Schnittstelle hÃ¤ngt und die Firmware noch antwortet. Monitor wÃ¤hrend Upload schlieÃŸen (`monitor_dtr/rts = 0` verhindert Reset-Loops im Monitor). Bei Boot-Loop oder fehlendem USB-JTAG weiterhin manuell: BOOT halten â†’ Reset â†’ BOOT loslassen.

USB-Monitor zeigt Elevation nur bei `#SETDEBUG,1$`. Protokoll-Tests Ã¼ber RS485 an GPIO21/20.
