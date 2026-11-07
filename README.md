<<<<<<< Updated upstream
# GS-Traffic2WIFI 4.0Beta1
=======
# GS-Traffic2WIFI 4.1BETA1
>>>>>>> Stashed changes

Complete rewrite of the GS-Traffic2WIFI firmware (Arduino IDE). It bridges a serial NMEA/FLARM source to WiFi (TCP) and, new in 4.0, converts the NMEA data into **GDL90** and broadcasts it via UDP for EFB apps. Hardware 3.0 (M5 Dial) is supported, with its own display driver and no external libraries.

## Hardware releases

|                                   | GS-Traffic2WIFIv1       | GS-Traffic2WIFIv2  | GS-Traffic2WIFIv3 |
|-----------------------------------|-------------------------|--------------------|-------------------|
| Chipset                           | ESP32                   | ESP32              | ESP32-S3 (M5 Dial) |
| Software compatibility            | 1.0 > now               | 3.0 > now          | 4.0 > now         |
| Support of Warn-LED (GPS+Traffic) | via soldering           | supported          | via LED ring / display |
| Screen + knob                     | not supported           | not supported      | supported         |
| Factory reset                     | jumper cable needed (GPIO 19 to GND at power-up) | via button | via screen / menu |
| Flash                             | WebUI + Micro-USB       | WebUI + programmer | WebUI + USB-C     |
| Debugging                         | via USB                 | via programmer     | via USB-C         |
| Support of RS232 TX               | supported               | supported          | not supported     |
| Support of Trafficmod LED         | not supported           | not supported      | supported         |
| Bluetooth LE (NMEA)               | not supported (does not fit the app partition) | not supported (does not fit the app partition) | supported |

## Functions

| Function                                              | v1 | v2 | v3 |
|-------------------------------------------------------|----|----|----|
| NMEA over TCP (port configurable)                     | yes | yes | yes |
| GDL90 over UDP (ownship, traffic, geometric altitude)  | yes | yes | yes |
| NMEA and GDL90 individually switchable, run in parallel | yes | yes | yes |
| GDL90 delivery: broadcast / unicast per device / both  | yes | yes | yes |
| Stratux heartbeat, optional ForeFlight ID              | yes | yes | yes |
| Strict NMEA (reject sentences without checksum)        | yes | yes | yes |
| WebUI (English / German), help texts, OTA update       | yes | yes | yes |
| WiFi region selection (29 regions), channel validation | yes | yes | yes |
| Client mode with fallback access point                 | yes | yes | yes |
| Simulator (6 aircraft, all FLARM alarm levels)         | yes | yes | yes |
| Red / orange status LED                                | yes | yes | -   |
| Display with wheel (flight data, GNSS, WiFi, info)     | -   | -   | yes |
| Traffic radar and traffic list on the display          | -   | -   | yes |
| WiFi QR code on the display                            | -   | -   | yes |
| Settings on the display (brightness, outputs, reset)   | -   | -   | yes |
| Trafficmod LED ring (direction of alarm traffic)       | -   | -   | yes |
| NMEA over Bluetooth LE (HM-10 / CC2541 compatible)     | -   | -   | yes |
| Aircraft databases (FLARMnet, OGN): registration in GDL90 and WebUI | yes | yes | yes |
| Flight log (start/end time, duration, start/end position), WebUI | yes | yes | yes |
| Flight log on the display (list, delete)               | -   | -   | yes |

## What 4.1BETA1 adds: aircraft databases and flight log

### Aircraft databases (registrations)
- Two **independent** databases can be loaded in the WebUI (Configuration, "Aircraft databases"): **FLARMnet** (`.tdb`) and **OGN** (`.csv` or `.json` from ddb.glidernet.org, entries marked "not identified" are skipped). Each has its own load and delete button, so each can be updated on its own. If an ID is in both, FLARMnet wins.
- The browser converts the file to a compact table (about 190 KB for 15,500 aircraft) and uploads it; the module stores it in the otherwise unused data partition of the standard partition scheme ("Default 4MB with spiffs", one third for FLARMnet, two thirds for OGN). No new partition scheme, no file system, no library. Firmware updates and factory reset keep the databases. If the partition is missing, the WebUI says so and the function stays off.
- For known FLARM/ICAO IDs the **registration** is sent as callsign in the GDL90 traffic report (letters and digits, no hyphen, 8 characters) and shown in the traffic list of the WebUI. Unknown IDs keep the hex ID.
- Optional (Compatibility): **add the registration to the FLARM NMEA stream** (TCP and Bluetooth) as `ID!REGISTRATION` in PFLAA, like Stratux and SoftRF. This is not part of the FLARM standard; some apps cope with it, others (for example Air Avionics displays) stop showing traffic. Off by default. The serial output is never changed.

### Flight log

Records every flight with date, start and end time (UTC), duration and the GPS position of take-off and landing (no track). All hardware versions record and show the log in the WebUI (tab "Flights"); hardware 3.0 also shows it in the display menu.

- **Detection:** a flight starts when the ground speed stays above the threshold for 10 s and ends when it stays below it for 60 s. Flights shorter than 2 minutes are ignored. The times come from the GPS (RMC date and time), so the GPS must send GPRMC with a valid date. The simulator is not logged.
- **Settings:** function on/off (default on) and start/stop speed in km/h (default 60, range 20-300), in the WebUI and, on hardware 3.0, in the display menu (Flight log, Log km/h).
- **Storage:** the last 50 flights are kept in flash, the oldest is overwritten. The running flight is saved every minute, so a power loss after landing still leaves an entry. The log has its own NVS namespace (`gs4log`): a factory reset does **not** delete it, only "Delete" (single flight) or "Delete all" in the WebUI or the display menu.
- **Export:** the "Flights" tab of the WebUI has an "Export CSV" button (date, start, end, duration in seconds, start/end position; UTC, comma separated). The file is built in the browser, no firmware space is used.
- **Display (hardware 3.0):** own page "Flights" after Info (press the wheel to open, turn to browse): date, UTC start and end time, duration and start/end position; click = delete this flight (with confirmation), last entries = "Delete all" and "Back". Function on/off and speed are in Settings (Flight log, Log km/h).

## What 4.0Beta1 brings

### GDL90 output (UDP, default port 4000)
- Generated from the NMEA stream:
  - Ownship from `RMC`, `GGA`, `GSA`, `PGRMZ` (pressure altitude, GPS MSL altitude as fallback)
  - Traffic from FLARM `PFLAA` (relative position converted to lat/lon), alarm level from `PFLAA`/`PFLAU`
  - Messages: Heartbeat (0x00), Ownship (0x0A), Ownship Geometric Altitude (0x0B), Traffic (0x14), Stratux heartbeat (0xCC), optional ForeFlight ID (0x65)
- One heartbeat/ownship/altitude cycle per second; traffic reports are spread over the second.
- The CRC follows the GDL90 ICD (`crc = Table[crc >> 8] ^ (crc << 8) ^ byte`), checked against the ICD example heartbeat.
- Delivery: UDP broadcast (default), unicast to every connected device (Stratux style) or both. Packets are sent from the GDL90 port itself.
- The status page has a panel "GDL90 as sent" (heartbeat flags, ownship position, altitudes, NIC/NACp, counters).
- While the simulator runs the heartbeat does not claim a valid UTC time.
- Verified with SkyDemon.

### NMEA
- NMEA over TCP and GDL90 over UDP run in parallel and are switchable independently.
- Sentences without checksum are accepted by default, bad checksums are always dropped; "Strict NMEA" rejects sentences without checksum. The status page shows the last rejected lines (B: bad checksum, N: no checksum).

### Stability and scaling
- Bounded, allocation-free NMEA line assembler; truncated or corrupt sentences are counted and dropped.
- The web server runs in its own task; the data path never waits for HTTP.
- Slow TCP clients are dropped instead of stalling the bridge; saving settings no longer clears the NVS namespace.
- Client mode is non-blocking; if joining fails the unit opens a fallback AP with the factory credentials.
- Fixed-size tables (40 traffic targets, 10 TCP clients); the logic is split into small modules.

### WebUI
- English / German (button in the header, remembered in the browser, default follows the browser language).
- Help text for every setting (switchable), strict validation with clear error messages, WiFi region with matching channel list, "Saving..." feedback.
- Ownship identity (address, address type, callsign, emitter category) grouped as advanced settings.
- LED ring on/off and brightness, Bluetooth LE on/off and name (hardware 3.0).

### Hardware 3.0
- Build for v3 ESP32-S3. Own GC9A01 driver, own fonts (`tools/genfont.py`), interrupt-driven wheel, buzzer click and a bit-banged WS2812 driver. No extra libraries.
- **Wheel:** turn = next/previous page, click = action of the page, long press (0.8 s) = back to the first page / leave the menu.
<<<<<<< Updated upstream
- **Pages:** Flight data (ground speed, altitude, track) - GNSS (satellites, position state, UTC, HDOP) - Traffic radar (click = range 1/2/5/10 NM, track-up while moving, targets coloured by FLARM alarm level, relative altitude in 100 ft) - Traffic list (5 most important targets: alarm first, then distance; relative altitude, climb/sink) - WiFi (SSID, key, IP, clients) - WiFi QR (scan with the phone camera to join the WiFi) - Info (firmware, hardware) - Settings.
- **Settings menu** (press the wheel to open): display brightness, LED ring on/off, LED brightness, NMEA TCP, NMEA BLE, GDL90 UDP, factory reset (with confirmation), back. Changing an output saves and restarts; brightness is applied live.
- **Trafficmod LEDs** (25 x WS2812, GPIO 13): LED 0 = status (green / red alarm / orange no position), LEDs 1-24 = direction of traffic within 5 NM relative to the track; only targets with an alarm are shown (yellow / orange / red = FLARM alarm level 1 / 2 / 3).
=======
- **Pages:** Flight data (ground speed, altitude, track) - GNSS (satellites, position state, UTC, HDOP) - Traffic radar (click = range 1/2/5/10 NM, track-up while moving, targets coloured by FLARM alarm level, relative altitude in 100 ft) - Traffic list (5 most important targets: alarm first, then distance; relative altitude, climb/sink) - WiFi (SSID, key, IP, clients) - Bluetooth (name, connected phones) - WiFi QR (scan with the phone camera to join the WiFi) - Info (firmware, hardware) - Flights (flight log) - Settings.
- **Settings menu** (press the wheel to open): display brightness, LED ring on/off, LED brightness, start page, NMEA TCP, NMEA BLE, GDL90 UDP, flight log on/off, log speed, restart, factory reset (with confirmation), back. Changing an output saves and restarts; brightness and start page are saved without a restart.
- **Trafficmod LEDs** (25 x WS2812, GPIO 13): at power-up a comet runs once around the ring (1.8 s) and changes its colour on the way (only if the ring is switched on).
- **Trafficmod LEDs, operation:** LED 0 = status (green / red alarm / orange no position), LEDs 1-24 = direction of traffic within 5 NM relative to the track; only targets with an alarm are shown (yellow / orange / red = FLARM alarm level 1 / 2 / 3).
>>>>>>> Stashed changes

### Bluetooth LE (Hardware 3.0)
- The module behaves like a CC2541 / HM-10 serial adapter: service `0xFFE0`, characteristic `0xFFE1` (notify, write without response), 20 byte notifications, same NMEA stream as TCP. Name configurable (default = default SSID), off by default, applies after reboot, runs in parallel to WiFi.

## Build

Arduino IDE, Arduino-ESP32 core 2.0.x or 3.x. No extra libraries.

## Layout

```
GS-Traffic2WIFI/     Arduino sketch (open GS-Traffic2WIFI.ino)
  Nmea.*             line assembler, checksum, sentence parsers     pure C++
  State.*            ownship/traffic state, LED logic               pure C++
  Gdl90.*            GDL90 encoder + 1 Hz publisher                 pure C++
  Simulator.*        synthetic NMEA for testing                     pure C++
  Settings.*         NVS persistence, migration from 3.x
  Bridge.*           serial / TCP / UDP / BLE data path
  WebUi.*, WebPage.h WebUI, JSON API, OTA update
  StatusLed.*, App.* LED, shared runtime state
  Ble.*              NMEA over Bluetooth LE
  CallsignDb.*       registration database lookup, PFLAA rewrite    pure C++
  CallsignStore.*    database in the data partition (upload, erase)
  FlightLog.*        flight detection and log                       pure C++
  FlightStore.*      flight log in NVS (own namespace), thread-safe access
  DialUi.*, Gfx.*, Qr.*  HW 3.0 pages, menu, LED ring, 2D renderer, QR     pure C++
  DialHw.*, DialFont.*  HW 3.0 display/wheel/LED drivers, fonts (ESP32-S3 only)
tools/               gdl90_monitor.py, genfont.py
test/                host tests + syntax check of the ESP32 glue
```

Threading: `loop()` owns the navigation state (writes under a mutex); the web task and the display task only read snapshots. Settings are read-only at runtime; the WebUI saves and reboots.

## GDL90 notes

- Position valid/UTC flags in the heartbeat follow the age of the last valid `RMC` (3 s).
- Without a valid fix, ownship is still sent (NIC = 0, position 0/0) as the spec requires, so apps show "no GPS".
- NIC is fixed at 8 like Stratux; NACp is estimated from HDOP (HDOP x 5 m). Traffic uses NIC 8 / NACp 8.
- "Airborne" is derived from ground speed with hysteresis (on at 30 kt, off below 15 kt).
- Ownship vertical rate is reported as "unknown"; traffic vertical rate comes from `PFLAA`.
- Traffic altitude = ownship pressure altitude + relative vertical distance from `PFLAA`.
- FLARM aircraft types are mapped to GDL90 emitter categories. Traffic callsign is the 6-digit FLARM/ICAO id.
- Traffic is dropped 6 s after the last `PFLAA` update.
- Datagrams are only sent while at least one station is associated (AP mode).

## Settings changes compared to 3.x

- Settings use a new NVS namespace (`gs4`). On first boot SSID, key, region, baud rate, ports, gateway/power/client flags and LED options are imported from 3.x automatically.
- "Extract data from NMEA" is gone: parsing is always on.
- LED reset cycles (counted in NMEA messages) became hold times in seconds.
- Client mode connects to a configurable server address instead of the hard-coded 192.168.1.1.
- The simulator replaces the old fake modes; it feeds the full pipeline (TCP, serial, GDL90, BLE, LEDs).
- Factory reset: hold GPIO 19 to GND at power-up (hardware 1/2) or use the display menu (hardware 3); the WPA key is no longer exposed by the web API.

## Not verified yet

- GDL90 has been verified with SkyDemon only.
- NIC 8 / NACp from HDOP is not confirmed in SkyDemon (fallback would be NIC 10 / NACp 9).

## Version history

GS-Traffic2WIFI software 1.0 and 1.1 was a stripped down fork of [ESP32-Serial-Bridge](https://github.com/AlphaLima/ESP32-Serial-Bridge). With version 2.0 there are a lot of extensions and the codebase was reworked in most parts; version 2.0 and higher offer a WebUI for configuring the device. Software 1.1 was the first official release and was shipped with GS-Traffic2WIFI modules offered by [gs-avionik.de](https://www.gs-avionik.de).

<<<<<<< Updated upstream
**4.0Beta1** - complete rewrite, see above (GDL90, Bluetooth LE, hardware 3.0 with display, new WebUI).
=======
**4.1BETA1** - aircraft databases (FLARMnet, OGN: registration in GDL90, optional in the NMEA stream) and flight log: date, UTC start/end time, duration and start/end position of the last 50 flights, configurable start/stop speed, WebUI on all hardware versions, display menu on hardware 3.0.

**4.0 (stable)** - complete rewrite, see above: GDL90 output, NMEA over Bluetooth LE, hardware 3.0 (M5 Dial) with display, traffic radar, LED ring and WiFi QR code, new WebUI in English and German.
>>>>>>> Stashed changes

**3.0c** - minor changes
- Modified serial code to solve some of the 3.0b bugs.

**3.0b** - minor changes
- Changed read-MAC function to be compatible with newer ESP32 libraries.
- WiFi security is now WPA2-PSK, so Apple devices no longer show an "unsecure" warning.

**3.0** - stable, offers all features of the 3.0 betas and
- Fixed: country code for WiFi mode changed to DE.

**3.0 BETA2**
- Added: experimental fake mode for serial output (replaces the dedicated faker firmware used for development).
- Fixed: update cycles are 0 in the WebUI when updating from a version before 3.0 and no default settings are loaded.
- Fixed: the update text shows "unknown state" instead of "failure" for updates that finish very quickly.

**3.0 BETA**
- Fixed: saving settings uses POST instead of GET for changed parameters.
- Fixed: reset settings now always works.
- Fixed: minor WebUI changes for better usability.
- Added: support for 2.0 hardware (dev devices).
- Added: many more details in `/gs.json` for debugging.
- Added: compile date of the firmware in the WebUI.
- Added: virtual status LED in the WebUI (works like the real status LED).
- Added: orange LED warning if no valid position is available, configurable in the WebUI.
- Added: new LED boot-up sequence.
- Added: experimental debug mode via the WebUI (mostly usable on 2.0 devices).
- Rewritten code for the traffic warning LED, more options in the WebUI.

**2.3b** - stable
- Added: firmware update via the WebUI.
- Added: tweaks for future hardware versions.
- Added: tooltips in the WebUI for all config options.
- Increased the maximum number of simultaneous NMEA clients to 10.

**2.2** - stable
- Added: experimental client mode for WiFi and TCP bridging.
- Added: code for a dual-colour LED on GPIO 18 and GPIO 5 for module status and/or traffic warning (configurable, based on `$PFLAU`).
- Added: baud rates 115200 and 230400.

**2.1** - stable
- Added: extracting data from `$GPGSA` and `$PGRMZ` messages.

**2.0** - stable
- WebUI (http://192.168.1.1) for configuring GS-Traffic2WIFI.
- Status pages and basic code for extracting and interpreting received NMEA data.
- Configurable baud rates and TCP ports.
- Configurable SSID / WPA key.
- Power-reducing mode for WiFi.
- NMEA GPRMC fake for testing (only one dummy message).

## License

See LICENSE. The WebUI no longer uses jQuery or Mini.css.