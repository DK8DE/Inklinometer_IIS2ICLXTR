# Inklinometer DK8DE IIS2ICLX - Setup and User Manual

**Firmware:** see [`src/version.h`](../src/version.h) or the Web UI status page  
**Language:** English · Deutsch: [`Anleitung_DE.md`](Anleitung_DE.md)

This document describes the **finished sensor unit** (board/enclosure with power input and RS485), commissioning, firmware flashing, the SoftAP web UI, and the RS485 text protocol.

---

## 1. Overview

The module measures **antenna elevation** using an ST **IIS2ICLX** (high-resolution 2-axis accelerometer) and an **ESP32-C3**. Angle readout and configuration are typically done over **RS485**. Optionally, a **Wi‑Fi SoftAP** provides a web UI for live angle, settings, and firmware update.

| Feature | Summary |
|---------|---------|
| Elevation | Filtered angle in degrees (0…360°, or limited 0…180°) |
| RS485 | Text protocol `#GET…$` / `#SET…$` at 115200 8N1 |
| SoftAP | Configuration, live angle, OTA update |
| Mechanics badge | Detects jolts/impacts (not slow rotation) |

---

## 2. Device connectors

### 2.1 Power supply

| Connection | Specification |
|------------|---------------|
| DC input | **7 … 36 V DC** |
| Polarity | Follow the silk-screen / connector marking on the board |

**Notes**

- Do not exceed the voltage range.
- Check polarity before connecting.
- After power-up the firmware starts on its own; the status LED (LBLED) begins blinking (see section 3).

### 2.2 RS485

| Terminal | Meaning |
|----------|---------|
| **A** | RS485 data line A (non-inverting, per transceiver marking) |
| **B** | RS485 data line B (inverting) |

**Bus parameters (protocol side)**

| Parameter | Value |
|-----------|-------|
| Baud rate | **115200** |
| Data bits | 8 |
| Parity | none (N) |
| Stop bits | 1 |
| Flow control | none |
| Termination | 120 Ω at the bus end if required by length/topology |

The on-board RS485 interface uses **automatic DE/RE control** (no separate direction pin).

### 2.3 LEDs

#### RS485 TX and RX LEDs

Two LEDs indicate RS485 traffic:

| LED | Meaning |
|-----|---------|
| **TX** | Lights/blinks briefly when the module **transmits** (replies on the bus) |
| **RX** | Lights/blinks briefly when the module **receives** (commands on the bus) |

Use them to verify that frames arrive and that the module answers.

#### LBLED (status / heartbeat)

The **LBLED** shows firmware state (not RS485 traffic):

| State | LED pattern |
|-------|-------------|
| Normal operation | **500 ms on / 500 ms off** |
| SoftAP / Wi‑Fi config active | **two short flashes**, then **~250 ms pause**, repeat |
| Fault (e.g. sensor not reachable) | **solid on** |
| Factory reset confirmed at boot | **3× fast blink**, then reboot |

---

## 3. Push-button (factory reset / SoftAP)

The unit has a **push-button** (wired to GPIO0, active low to GND, with pull-up). It has **two different functions**, depending on *when* it is pressed:

### 3.1 During runtime – toggle SoftAP (Wi‑Fi)

1. Device is already running (LBLED blinking normally).
2. Press the button **shortly and release**.
3. SoftAP is **toggled**:
   - was off → SoftAP **on** (LBLED switches to double-blink pattern)
   - was on → SoftAP **off** (LBLED returns to normal blink)

**SoftAP also turns off automatically** about **20 s** after the last Wi‑Fi client disconnects.

### 3.2 At power-up – factory reset

1. Remove power or prepare a reboot.
2. **Hold** the button pressed.
3. Apply power / reset so the button is still LOW during boot.
4. Firmware detects factory mode: clears NVS configuration and writes compile defaults.
5. LBLED confirms with **3× fast blinks**, then the device reboots with factory settings.

**Important:** This is a **configuration reset** (offset, filter, shock threshold, …), not a plain MCU reboot that keeps settings. Stored settings are lost.

---

## 4. Sensor mounting

For unambiguous elevation over the full range:

- Mount the sensor **vertically** in the elevation plane (both X/Y axes in the tilt plane).
- Hinge / rotation axis preferably perpendicular to the sensor face.
- Use full-scale **±1 g** (default) or **±2 g**; ±0.5 g is unsuitable for this mounting (saturation).

After mounting, typically:

1. Open SoftAP or use RS485.
2. Set mount rotation (`MOUNTROT`) and invert flags as needed.
3. Optionally run the 2-point level calibration in the web UI.

---

## 5. Flashing firmware

### Which method for what?

| Goal | Recommended path |
|------|------------------|
| Just update firmware (device already running) | **SoftAP OTA** (section 5.1) – no cable to the PCB |
| First install / recovery | Web flasher or PlatformIO via **Tag-Connect** (sections 5.2 / 5.3) |
| Further development / debugging | Tag-Connect + PlatformIO (section 5.3) |

> **Note:** For simply applying an update, use **SoftAP OTA**. The on-board USB/programming interface is intended for **development**, not for everyday end-user updates.

### 5.1 SoftAP OTA (recommended for updates)

1. Enable SoftAP with the button (LBLED → double blink).
2. Join Wi‑Fi `INKLINO-<UID>` (password in section 6).
3. Open `http://192.168.4.1` and log in.
4. Tab **Update** → upload `firmware.bin`.
5. The device reboots after success.

`firmware.bin` is included in the GitHub release / artifact for each version.

### 5.2 On-board programming connector (development only)

There is **no** end-user USB receptacle on the board. Instead:

| Element | Location / description |
|---------|------------------------|
| **Tag-Connect (6-pin)** | Programming / USB-serial footprint **below the microcontroller (ESP32-C3)** |
| **PROG** | Two **contact pads** to the **left** of the Tag-Connect – Boot/PROG |
| **RESET** | Two more **pads** further **left** – hardware reset |

Flashing by cable requires a matching **6-pin Tag-Connect plug** (and typically a USB adapter/programmer). PROG and RESET pads can be jumpered or touched with a probe when needed (hold BOOT → pulse Reset → release BOOT).

This path is **only recommended** if you want to **develop** the firmware or reflash the board from scratch – **not** for simply installing an update.

Downloads (web flasher / release):

| File | Use |
|------|-----|
| `factory.bin` | Full flash / first install / recovery (from address `0x0`) |
| `firmware.bin` | App only – SoftAP OTA (web tab **Update**) |

Web flasher (Chrome/Edge):  
**https://dk8de.github.io/Inklinometer_IIS2ICLXTR/**  
Install `factory.bin` there via the Tag-Connect USB adapter.

### 5.3 PlatformIO (development)

Requirement: Tag-Connect adapter connected (see 5.2). In the project directory:

```bash
# Linux / macOS / Git Bash
./build.sh                 # sync version, build, upload
./build.sh --no-upload
UPLOAD_PORT=COM34 ./build.sh

# Windows PowerShell
.\build.ps1
.\build.ps1 -NoUpload
.\build.ps1 -UploadPort COM34
```

Or manually:

```bash
pio run -e esp32-c3 -t upload
pio device monitor -b 115200
```

After changing the partition table, once:

```bash
pio run -e esp32-c3 -t erase -t upload
```

**Note:** Close the serial monitor during upload. On boot-loop: short PROG pads (BOOT) → pulse RESET → release PROG.

---

## 6. SoftAP and web UI

### 6.1 Connecting

| | |
|--|--|
| Start SoftAP | Short button press (LBLED → double blink) or via RS485 `#SETWIFI,1$` |
| SSID | `INKLINO-<UID>` (UID = last 3 MAC bytes, hex) |
| Wi‑Fi password | `Rotorconfig` |
| Web UI URL | `http://192.168.4.1` |
| Login | user `admin` / password `Rotorconfig` |

The RS485 protocol keeps running **in parallel** while SoftAP is active.

### 6.2 Tabs

| Tab | Contents |
|-----|----------|
| **Status** | Live elevation, **stable / jolt** badge, system info |
| **Configuration** | Mount, filter, calib offset, shock threshold, full-scale, debug, … |
| **Calibration** | 2-point level calibration |
| **Update** | OTA with `firmware.bin` |
| **Reset** | Reboot / factory reset |

Web UI language: German / English (stored in NVS).

### 6.3 Shock threshold and “stable / jolt” badge

The badge uses INT2 (sensor wake-up):

- **stable** = no acceleration change above the threshold
- **jolt** = impact / mechanical disturbance detected

This is **not** an indicator of slow angle motion. Sensitivity is set with **shock threshold** (`1…63`, default **25**): lower = more sensitive.

---

## 7. RS485 protocol – basics

### 7.1 Frame format

- Every command starts with `#` and ends with `$`.
- **No** line breaks (`\r` / `\n`) inside the frame.
- Between them: command name and optional value.

| Type | Format | Example |
|------|--------|---------|
| Read | `#GET<NAME>$` | `#GETDG$` |
| Write | `#SET<NAME>,<value>$` | `#SETFILTER,0.35$` |
| Success | `#ACK_<CMD>,<value>$` | `#ACK_GETDG,45.12$` |
| Error | `#NACK_<CMD>,ERR$` | `#NACK_SETFILTER,ERR$` |

On SET, the ACK contains the **accepted** value (after clamping/normalization when valid).

### 7.2 Typical session

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

Invalid value:

```text
#SETFILTER,2$
#NACK_SETFILTER,ERR$
```

### 7.3 Storage (NVS)

Every **successful SET** stores configuration in flash (NVS namespace `incl`). Values survive power loss. The same applies when saving from the web UI.

---

## 8. Command reference (detailed)

### 8.1 Measurement and mechanics

#### `GETDG` – elevation

- **Format:** `#GETDG$`
- **Reply:** `#ACK_GETDG,<angle>$`
- **Meaning:** Current filtered elevation angle in degrees.
- **Decimals:** controlled by `ELEVDEC` (0 / 1 / 2).
- **No SET** – read only.

#### `GETSETTLED` – mechanics stable?

- **Format:** `#GETSETTLED$`
- **Reply:** `#ACK_GETSETTLED,0$` or `,1$`
- **`1`** = stable (no shock above threshold)
- **`0`** = jolt/shock detected
- Matches the web badge “stable” when `1`.

#### `GETSHOCK` – shock detected? (inverted reading)

- **Format:** `#GETSHOCK$`
- **`1`** = jolt/shock
- **`0`** = stable
- Practically: `GETSHOCK = 1 - GETSETTLED`.

#### `GETSHOCKTHS` / `SETSHOCKTHS` – shock threshold

- **Read:** `#GETSHOCKTHS$`
- **Write:** `#SETSHOCKTHS,<1…63>$`
- **Default:** `25`
- **Effect:** IIS2ICLX `WAKE_UP_THS` register.  
  1 LSB ≈ full-scale / 64 (finer steps at ±1 g than at ±2 g).
- Applied **immediately** to the sensor; also stored in NVS.
- Lower value → more sensitive; higher → only harder impacts.

---

### 8.1a Jolt monitoring (error on too many jolts)

At runtime the firmware counts every **jolt** (INT2 changes from “stable” to “jolt”; sensitivity is set by `SHOCKTHS`). If at least a configurable **number** of jolts occur within a configurable **time window**, a **jolt error** is set. The window is *sliding* (always the last `SHOCKWIN` seconds).

The error:

- stays set until it is **cleared** (RS485 or web),
- is **not** stored in flash (cleared by reboot / power loss),
- does **not** affect the **LBLED** (which only shows sensor faults).

`SHOCKWIN` and `SHOCKLIM` are stored in NVS.

#### `GETSHOCKWIN` / `SETSHOCKWIN` – time window

- **Read:** `#GETSHOCKWIN$` · **Write:** `#SETSHOCKWIN,<1…3600>$`
- Unit: **seconds**. Default: `60`.

#### `GETSHOCKLIM` / `SETSHOCKLIM` – jolts until error

- **Read:** `#GETSHOCKLIM$` · **Write:** `#SETSHOCKLIM,<0…60>$`
- Number of jolts within the window that sets the error. **`0` = monitoring off.** Default: `5`.

#### `GETSHOCKCNT` – jolts in current window

- **Format:** `#GETSHOCKCNT$` → `#ACK_GETSHOCKCNT,<n>$`
- Read only; number of jolts within the last `SHOCKWIN` seconds.

#### `GETSHOCKERR` / `SETSHOCKERR` – query / clear jolt error

- **Query:** `#GETSHOCKERR$` → `#ACK_GETSHOCKERR,1$` (error active) or `,0$`
- **Clear:** `#SETSHOCKERR,0$` → `#ACK_SETSHOCKERR,0$`  
  (other values → `#NACK_SETSHOCKERR,ERR$`; the error can only be cleared, not set by command)
- Clearing restarts the jolt counter.

Example:

```text
#SETSHOCKWIN,30$
#ACK_SETSHOCKWIN,30$
#SETSHOCKLIM,3$
#ACK_SETSHOCKLIM,3$
#GETSHOCKERR$
#ACK_GETSHOCKERR,1$
#SETSHOCKERR,0$
#ACK_SETSHOCKERR,0$
```

The web UI (tab **Status**) shows “Jolts in window” and “Jolt error”; with an active error the **Clear jolt error** button appears. Window and count are set in the **Configuration** tab.

---

### 8.2 Angle presentation and filter

#### `GETELEVDEC` / `SETELEVDEC` – decimal places

- Values: `0`, `1`, or `2`
- Affects formatting of `GETDG` (and the web display).

#### `GETFILTER` / `SETFILTER` – EMA filter α

- Range: `0.01` … `1.0`
- Larger = angle follows faster (less smoothing).
- Internally scaled further:
  - when **stable**: α × 0.35 (stronger smoothing)
  - when **jolt**: α × 1.8 (faster)
- Default: `0.15`

#### `GETCALIB` / `SETCALIB` – calibration offset

- Range: −180.0 … +180.0 degrees
- Applied as an offset correction to the computed angle.
- Default: `0.0`

#### `GETLIMIT180` / `SETLIMIT180` – soft limit 0…180°

- `0` = full-circle behaviour of the firmware output pipeline
- `1` = clamp output to **0…180°**:
  - 0…180 → unchanged
  - >180 … ≤270 → **180**
  - >270 … <360 → **0**

#### `GETMOUNTROT` / `SETMOUNTROT` – mount rotation

- Allowed: `0`, `90`, `180`, `270`
- Rotates the logical 0° side of the mounting.
- Changing it restarts the elevation filter.

#### `GETINVROT` / `SETINVROT` – invert rotation direction

- `0` / `1`
- `1` = angle increases counter-clockwise (per firmware mounting convention).

#### `GETSWAPXY` / `SETSWAPXY` – swap axes

- `0` / `1` – swap sensor X/Y (mounting adaptation).

#### `GETINVREF` / `SETINVREF` – invert reference

- `0` / `1` – invert reference axis.

#### `GETINVSENS` / `SETINVSENS` – invert sense

- `0` / `1` – invert sense direction.

#### `GETFS1G` / `SETFS1G` – full-scale

- `1` = ±1 g (default, higher resolution)
- `0` = ±2 g
- Written **immediately** to the sensor.
- Also changes the real mg step size of the shock threshold (LSB ≈ FS/64).

---

### 8.3 Diagnostics

#### `GETWIFI` / `SETWIFI` – query / switch Wi‑Fi (SoftAP) on and off

- **Query:** `#GETWIFI$` → `#ACK_GETWIFI,1$` (SoftAP on) or `,0$` (off)
- **Switch on:** `#SETWIFI,1$` → `#ACK_SETWIFI,1$`
- **Switch off:** `#SETWIFI,0$` → `#ACK_SETWIFI,0$`
- Works like the **GPIO0** button, but explicit: `1` only starts the SoftAP if it is off; `0` only stops it if it is on (no toggling). Repeated commands are therefore harmless.
- The reply contains the **actual** state after the command.
- Other values → `#NACK_SETWIFI,ERR$`.
- **Switching off** disconnects connected clients immediately.
- The state is **not** stored in NVS: after a restart Wi‑Fi is always off. The automatic shutdown (about 20 s after the last client disconnects) also applies to a SoftAP started via RS485.
- Credentials and address are unchanged: SSID `INKLINO-<UID>`, password `Rotorconfig`, `http://192.168.4.1`.

Example:

```text
#GETWIFI$
#ACK_GETWIFI,0$
#SETWIFI,1$
#ACK_SETWIFI,1$
#SETWIFI,0$
#ACK_SETWIFI,0$
```

#### `GETDEBUG` / `SETDEBUG` – USB debug

- `1` = print elevation ~10×/s on USB-CDC (115200 baud)
- `0` = off
- Independent of the RS485 protocol; useful with a USB serial monitor.

---

### 8.4 Summary table

| Meaning | GET | SET | Values |
|---------|-----|-----|--------|
| Elevation | `GETDG` | — | degrees |
| Stable (no shock) | `GETSETTLED` | — | 0 / 1 |
| Shock detected | `GETSHOCK` | — | 0 / 1 |
| Shock threshold | `GETSHOCKTHS` | `SETSHOCKTHS` | 1…63 (default 25) |
| Jolt window (s) | `GETSHOCKWIN` | `SETSHOCKWIN` | 1…3600 (default 60) |
| Jolts until error | `GETSHOCKLIM` | `SETSHOCKLIM` | 0…60, 0 = off (default 5) |
| Jolts in window | `GETSHOCKCNT` | — | count |
| Jolt error | `GETSHOCKERR` | `SETSHOCKERR,0` (clear) | 0 / 1 |
| Swap axes | `GETSWAPXY` | `SETSWAPXY` | 0 / 1 |
| Invert REF | `GETINVREF` | `SETINVREF` | 0 / 1 |
| Invert SENSE | `GETINVSENS` | `SETINVSENS` | 0 / 1 |
| Invert rotation | `GETINVROT` | `SETINVROT` | 0 / 1 |
| Mount rotation | `GETMOUNTROT` | `SETMOUNTROT` | 0 / 90 / 180 / 270 |
| Limit 0…180° | `GETLIMIT180` | `SETLIMIT180` | 0 / 1 |
| Calib offset | `GETCALIB` | `SETCALIB` | −180.0…+180.0 |
| Full-scale ±1 g | `GETFS1G` | `SETFS1G` | 0 / 1 |
| Decimal places | `GETELEVDEC` | `SETELEVDEC` | 0 / 1 / 2 |
| Filter α | `GETFILTER` | `SETFILTER` | 0.01…1.0 |
| Wi‑Fi (SoftAP) | `GETWIFI` | `SETWIFI` | 0 / 1 |
| USB debug | `GETDEBUG` | `SETDEBUG` | 0 / 1 |

---

## 9. First commissioning checklist

1. Apply **7–36 V DC** (check polarity).
2. LBLED: normal blink = OK; solid on = check sensor fault.
3. Connect **RS485 A/B** to the controller (baud 115200 8N1).
4. Test: send `#GETDG$` → RX LED briefly active, then TX LED and `#ACK_GETDG,…$`.
5. Optional SoftAP: short button press → Wi‑Fi `INKLINO-…` → `http://192.168.4.1` → login.
6. Set mount/offset/filter/shock threshold and store (SET or web **Save**).
7. Optional: 2-point calibration in web tab **Calibration**.

---

## 10. 2-point level calibration (web)

1. Place the sensor on a level surface → **Measurement 1**.
2. Rotate ~**180°** in place horizontally → **Measurement 2**.
3. Both raw angles are typically close (~90°).
4. Offset ≈ `90° − mean` → **Apply offset** (NVS).

The 180° turn averages small asymmetries of the contact surface.

---

## 11. Factory defaults (compile defaults)

After factory reset (button at boot or web Reset tab):

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
| `SHOCKWIN` | 60 s |
| `SHOCKLIM` | 5 |
| `DEBUG` | 0 |
| UI language | de |

---

## 12. Troubleshooting

| Observation | Likely cause / action |
|-------------|------------------------|
| LBLED solid on | Sensor I²C fault; check wiring/sensor, reboot |
| No RS485 reply | A/B swapped, baud rate, common ground, termination |
| RX LED blinks, no TX LED | Invalid frame (missing `#`/`$`, line break) |
| SoftAP not visible | Short button press; check LBLED double-blink |
| SoftAP disappears alone | Normal: ~20 s after last client disconnects |
| Badge stays “stable” during slow motion | Expected – jolts only; lower threshold if needed |
| Wrong angle after power cycle | Check mounting/calibration/`MOUNTROT` |
| SoftAP OTA fails | SoftAP on? File is `firmware.bin` (not factory)? |
| Tag-Connect flash fails | Adapter seated? PROG/RESET pads (BOOT mode)? Monitor closed? |

---

## 13. Further links

- Developer README: [`../README.md`](../README.md)
- German manual: [`Anleitung_DE.md`](Anleitung_DE.md)
- Web flasher: https://dk8de.github.io/Inklinometer_IIS2ICLXTR/
