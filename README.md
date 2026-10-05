# Inklinometer IIS2ICLX – ESP32-C3

Hochauflösende Antennen-Elevation über ST IIS2ICLX, Abfrage und Konfiguration per RS485-Textprotokoll. Einstellungen werden im NVS gespeichert.

## Hardware

| Signal | ESP32-C3 |
|--------|----------|
| SDA | GPIO4 |
| SCL | GPIO5 |
| RS485 TX | GPIO21 (UART0) |
| RS485 RX | GPIO20 (UART0) |
| USB | nativer USB-CDC (optionaler Debug) |

- I2C: 400 kHz / Sensor je nach SA0 (Firmware: `0x6B`)
- RS485-Wandler mit **automatischer** DE/RE-Umschaltung (kein DE-Pin nötig)
- Baudrate Protokoll: **115200 8N1**

## Serielle Schnittstellen

| Port | Verwendung |
|------|------------|
| UART0 (GPIO21/20) | RS485-Protokoll (`#GET…$` / `#SET…$`) |
| USB-CDC | Debug-Ausgabe, nur wenn `#SETDEBUG,1$` |

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
| Achsen tauschen (swapXY) | `GETSWAPXY` | `SETSWAPXY` | `0` / `1` |
| REF invertieren | `GETINVREF` | `SETINVREF` | `0` / `1` |
| SENSE invertieren | `GETINVSENS` | `SETINVSENS` | `0` / `1` |
| Drehrichtung invertieren | `GETINVROT` | `SETINVROT` | `0` / `1` (1 = gegen Uhrzeigersinn vergrößert) |
| Soft-Limit 0…180° | `GETLIMIT180` | `SETLIMIT180` | `0` / `1` |
| Kalibrier-Offset | `GETCALIB` | `SETCALIB` | −180.0 … +180.0 |
| Full-Scale ±1 g | `GETFS1G` | `SETFS1G` | `0` = ±2 g, `1` = ±1 g |
| Anzeige-Nachkommastellen | `GETELEVDEC` | `SETELEVDEC` | `0` / `1` / `2` |
| EMA-Filter alpha | `GETFILTER` | `SETFILTER` | 0.01 … 1.0 (größer = schneller) |
| USB-Debug | `GETDEBUG` | `SETDEBUG` | `0` / `1` |

### Soft-Limit (`LIMIT180`)

Wenn aktiv (`1`):

- Rohwinkel 0…180 → unverändert
- >180 … ≤270 → Ausgabe **180**
- >270 … <360 → Ausgabe **0**

### NVS

Jeder erfolgreiche **SET**-Befehl speichert die Konfiguration im Flash (Namespace `incl`). Nach Reset bleiben die Werte erhalten.

## Montage (Elevation)

Sensor **senkrecht** in der Elevationsebene montieren (beide Achsen X/Y in der Kippebene). Scharnier senkrecht zur Sensorfläche. Dann ist `atan2` über 0…360° eindeutig (auch nach Power-Cycle).

**Hinweis:** Full-Scale ±0.5 g ist für diese Montage ungeeignet (Sättigung bei ca. 45°/135°). Nutze ±1 g oder ±2 g.

## Build / Flash

```bash
pio run -t upload
pio device monitor
```

USB-Monitor zeigt Elevation nur bei `#SETDEBUG,1$`. Protokoll-Tests über RS485 an GPIO21/20.
