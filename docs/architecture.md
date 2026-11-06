# AAPI System Architecture & Protocol Spec

**Status:** Draft v0.1 (2026-11-03). See `decisions.md` for the reasoning behind each choice (D-xxx).
Numbers marked **[TBD-test]** are placeholders until the LoRa range test or bench tests give real values.

---

## 1. System overview

```
┌─────────────────────── BASE STATION ────────────────────────┐
│  Base Pi (4B or laptop)                                     │
│   ├─ base_core: fleet manager, scheduler, protocol (Python) │
│   ├─ dashboard (web UI)                                     │
│   ├─ upload server + data store (SQLite, CSV export)        │
│   └─ Wi-Fi access point (for docked uploads)                │
│        │ USB serial                                         │
│  Heltec V3 — "gateway" firmware role                        │
└────────┼────────────────────────────────────────────────────┘
         │ LoRa 915 MHz: commands, beacon/heartbeat, status
         │ (small packets only; never photos)
┌────────┼──────────────────── ROVER ─────────────────────────┐
│  Heltec V3 (ESP32-S3) — "rover" firmware role               │
│   ├─ LoRa radio (passes packets to the Pi; handles STOP     │
│   │    itself)                                              │
│   ├─ drive motor speed control (PID) + encoder              │
│   ├─ steering servo, camera pan servo (if used)             │
│   ├─ IMU (BNO085) → odometry in the odom frame              │
│   ├─ safety watchdog (Pi heartbeat, LoRa heartbeat, E-stop  │
│   │    sense, probe-up interlock)                           │
│   └─ battery voltage, charge-contact detect                 │
│        │ UART (3.3 V, direct)                               │
│  Rover Pi (Zero 2 W or 3B)                                  │
│   ├─ rover_core: protocol + mission state machine (Python)  │
│   ├─ navigation: map pose, Pure Pursuit, docking approach   │
│   ├─ vision: camera, AprilTag detection, plant photos       │
│   ├─ storage: SQLite + JPEG files                           │
│   └─ uploader: Wi-Fi to the base when docked                │
│                                                             │
│  Hardware E-stop ──► cuts 12 V motor/actuator power directly│
└─────────────────────────────────────────────────────────────┘
```

### 1.1 Who does what

| Job | Where | Why |
|---|---|---|
| Motor speed loop, encoder counting, IMU reading, odometry | ESP32 | Needs precise timing (hard real-time) |
| Safety stop: LoRa STOP, heartbeat loss, Pi loss, probe interlock | ESP32 | Must work even if the Pi crashes or hangs |
| LoRa protocol, mission logic, fleet logic | Pi (Python) | The simulation reuses the same code (D-010, D-011) |
| AprilTag detection, map pose, path following | Pi | Needs the camera and more computing power |
| Photo capture, storage, upload | Pi | Needs the file system and Wi-Fi |

**Deliberate exception to "the ESP32 only passes packets along":** the ESP32 reads the type byte of every LoRa packet it receives. It handles **STOP** itself and watches the **BEACON** timeout itself. Both still get forwarded to the Pi as well. Remote stop therefore doesn't depend on the Pi being alive.

---

## 2. Rover modes (state machine)

```
          charge detected                   MISSION_START (battery ok)
  BOOT ──────────────────► DOCKED ──────────────────────────────┐
    │                       ▲  │ (charging, uploading)          ▼
    │ no charge             │  │                         ┌── MISSION ──┐
    ▼                       │  │                         │ NAVIGATING  │
  IDLE ◄────────────────────┼──┘ undock/cmd              │     ↓       │
    │  ▲                    │                            │ SAMPLING    │
    │  │ deadman / exit     │ dock contacts made         │ (aim, photo,│
    ▼  │                    │                            │ probe↓, read│
  TELEOP                 DOCKING ◄── RETURNING ◄─────────│ probe↑ ok)  │
                       (tag-guided)    ▲                 └─────────────┘
                                       │ mission done / battery ≤ 25% /
                                       │ RETURN_HOME / link lost (autonomous)

  Reachable from any mode:  STOPPED (remote STOP; RESUME → previous mode)
                            ESTOP   (hardware button; needs button reset + RESUME)
                            FAULT   (see §6; needs operator action)
```

- **SAMPLING** finishes only when the probe-up switch confirms the probe is retracted. Until then the ESP32 **refuses drive commands** (safety interlock, ISO 13849-1 row in the proposal).
- **TELEOP** has a deadman: if no TELEOP command arrives for 1 s, the rover stops and goes to IDLE.
- While the probe is undecided (D-008), SAMPLING calls a stand-in probe/sensor interface that returns fake readings.
- The camera aim step does nothing if the camera ends up fixed (D-007).

---

## 3. LoRa physical layer

### 3.1 Time on air (in ms; 4/5 coding rate, 8-symbol preamble, explicit header, CRC on)

| Settings | 7 B (ACK) | 11 B (BEACON) | 18 B (STATUS) | 36 B (WAYPOINTS) |
|---|---|---|---|---|
| SF7 / 125 kHz | 36 | 41 | 52 | 77 |
| SF9 / 125 kHz | 124 | 144 | 185 | 267 |
| SF10 / 125 kHz | 248 | 289 | 330 | 494 |
| SF7 / 500 kHz | 9 | 10 | 13 | 19 |
| SF9 / 500 kHz | 31 | 36 | 46 | 67 |

(Calculated from the Semtech time-on-air formula. A higher SF (spreading factor) gives more range but slower packets.)

### 3.2 Proposed starting settings (D-012, Proposed)
- **Starting point: SF9, 500 kHz bandwidth, 4/5 coding rate, +14 to +20 dBm transmit power.** The range test then sweeps **SF7–SF10 at both 125 and 500 kHz** to pick the final setting.
- **FCC note (confirm with your advisor):** Under 47 CFR §15.247, a single fixed channel at 125 kHz bandwidth doesn't count as "digital modulation", which needs ≥ 500 kHz. Using 125 kHz would mean frequency hopping (≥ 50 channels, ≤ 0.4 s on any channel per 20 s). At **500 kHz bandwidth** the rover can stay on one channel without hopping, which is much simpler. That's the main reason for the proposed starting point. In any case, **keep every packet under 400 ms on air.**
- Use a **private sync word** and the network ID byte (§4.1), so the rover ignores other LoRa traffic nearby (e.g. Meshtastic users).

### 3.3 Time slots so rovers don't collide (TDMA)

The base drives a repeating **5 s cycle**. The BEACON doubles as the heartbeat the proposal requires every 5 s.

```
|BEACON| slot R1 | slot R2 | ... | slot Rn |  command window (~1 s)  |
 0 s                                                             5 s
```
- Each rover sends in its assigned slot only: one STATUS, plus optionally one ACK or EVENT.
- **Command window:** the base sends one command and the addressed rover ACKs right away. A command that wasn't ACKed is retried in the next cycle (up to 3 tries).
- **STOP and TELEOP are exceptions.** The base may send them at any time. STOP is sent 3 times, 250 ms apart. A collision is acceptable because of the repeats and the heartbeat-loss fallback.
- **Capacity:**
  - SF9/125 kHz: about 400 ms per slot, so about **9 rovers** per 5 s cycle.
  - SF9/500 kHz: about 150 ms per slot, so about **25 rovers**.
  - Either is plenty for the simulation and the demo.

---

## 4. LoRa message protocol

### 4.1 Packet header (5 bytes)

| Byte | Field | Notes |
|---|---|---|
| 0 | `net_id` | Network ID. Drops packets from other systems. |
| 1 | `type` | See message table below. |
| 2 | `src` | 0x00 = base, 0x01–0xFE = rover ID |
| 3 | `dst` | 0xFF = broadcast |
| 4 | `seq` | Increments per sender; used to spot duplicates and match ACKs |

The LoRa radio adds its own CRC, so corrupted packets are dropped in hardware. Multi-byte fields are little-endian. Positions are in **cm, int16** (±327 m), in the **map frame** (§7).

### 4.2 Base → rover

| Type | Name | Payload | Size | ACK? |
|---|---|---|---|---|
| 0x01 | BEACON | time u32 (Unix s), cycle u8, flags u8 (fleet-stop, …) | 11 B | no |
| 0x02 | STOP | reason u8 | 6 B | no, repeated ×3 |
| 0x03 | RESUME | — | 5 B | yes |
| 0x10 | MISSION_LOAD | mission_id u8, wp_count u8, start_time u32 (0 = now) | 11 B | yes |
| 0x11 | WAYPOINTS | mission_id u8, start_idx u8, n u8, n × (x i16, y i16, action u8) | ≤ 38 B (n ≤ 6) | yes |
| 0x12 | MISSION_START | mission_id u8 | 6 B | yes |
| 0x13 | MISSION_ABORT | mission_id u8 | 6 B | yes |
| 0x14 | RETURN_HOME | — | 5 B | yes |
| 0x20 | TELEOP | speed i8 (cm/s), steer i8 (deg) | 7 B | no (deadman) |
| 0x30 | SET_PARAM | param_id u8, value i32 | 10 B | yes |

`action` bits: photo, probe, and aim-camera-left/right.

### 4.3 Rover → base

| Type | Name | Payload | Size |
|---|---|---|---|
| 0x40 | STATUS | mode u8, x i16, y i16, heading u8 (256 steps ≈ 1.4°), battery % u8, mission_id u8, wp_idx u8, faults u16, pose_age u8 (s since last tag fix), beacon RSSI i8 | 18 B |
| 0x41 | ACK | acked_seq u8, result u8 (ok / rejected / busy) | 7 B |
| 0x42 | EVENT | code u8 (wp reached, sample done, docked, …), wp_idx u8, val1 i16, val2 i16 (e.g. moisture, temp summary) | 11 B |

### 4.4 Reliability rules
- Commands are **idempotent**: if the same seq arrives twice, the rover re-sends its ACK but doesn't act twice.
- The base retries a command that wasn't ACKed up to 3 cycles, then marks it failed on the dashboard.
- **Known limitation:** there's no message authentication, so anyone with a LoRa radio could send commands. That's acceptable for the prototype. A truncated HMAC (4 bytes) could be added later.

---

## 5. Pi ↔ ESP32 serial link

- UART at 3.3 V on both sides (direct wiring + ground), **460800 baud** [TBD-test].
- Framing: each frame is `type u8 | payload | CRC16-CCITT`, encoded with COBS and ended with a `0x00` byte. A dropped byte loses one frame, not the rest of the stream.

| Direction | Message | Rate / trigger |
|---|---|---|
| Pi → ESP | HEARTBEAT | 10 Hz |
| Pi → ESP | DRIVE (speed cm/s i16, steer centi-deg i16) | 20 Hz while moving |
| Pi → ESP | SERVO (id, angle) | as needed (camera pan) |
| Pi → ESP | PROBE (down / up / stop) | as needed (later, D-008) |
| Pi → ESP | LORA_TX (raw packet) | as needed |
| Pi → ESP | CONFIG (rover ID, slot offset, LoRa settings, limits) | at boot |
| ESP → Pi | ODOM (t_ms u32, x, y, θ, v in the odom frame) | 50 Hz |
| ESP → Pi | LORA_RX (raw packet, RSSI, SNR) | on receive |
| ESP → Pi | HW_STATUS (battery mV, E-stop, probe-up switch, charge detect, fault bits) | 2 Hz + on change |

**ESP32 safety rules (in firmware; they don't depend on the Pi):**
1. No Pi HEARTBEAT for **300 ms** → stop motors, set fault `PI_LOST`.
2. LoRa STOP received → stop motors immediately, then forward it to the Pi.
3. Drive commands are ignored unless the probe-up switch reads "up" *(once the probe exists)*.
4. Speed is capped at **1 m/s** in firmware, whatever the Pi asks for.
5. If the Pi has been silent for > 5 s, the ESP32 sends a basic STATUS with `PI_LOST` in the rover's slot (the slot offset was set via CONFIG), so the base knows the reason.

---

## 6. Failure handling

| Failure | Detected by | Response |
|---|---|---|
| LoRa link lost (rover side) | No BEACON for 3 cycles (15 s) | TELEOP: already stopped by the deadman. Supervised test: **stop**. Autonomous mission: finish the current waypoint, then RETURNING. |
| Rover goes silent (base side) | 3 missed STATUS → SUSPECT; 6 missed (30 s) → LOST | Dashboard alert at its last known position. **Reassign remaining waypoints** to an available rover (idle or docked, battery above threshold). |
| "Zombie" rover returns after being marked LOST | STATUS arrives from a LOST rover | Base sends MISSION_ABORT + RETURN_HOME so waypoints aren't sampled twice. |
| Pi crash / hang | ESP32 heartbeat timeout (300 ms) | Motors stop; `PI_LOST` reported over LoRa by the ESP32. |
| Low battery | Battery ≤ 25% | RETURNING. |
| Probe stuck down | Probe-up switch not "up" after timeout | FAULT; drive stays locked; alert. |
| Lost localization | `pose_age` > limit **and** distance driven since last tag > limit [TBD-test] | Slow down, look for a tag (pan the camera / rotate along the path); FAULT if none is found. |
| Remote stop | STOP packet | Stop within the 2 s target. Stop delay is about 1 s worst case: 3 repeats plus airtime. |
| Physical E-stop | Hardware | Cuts motor power directly (≤ 0.5 s); the ESP32 senses it and reports ESTOP. |
| Base reboots | Rovers see missing BEACONs | Same as link lost. Missions resume once BEACON returns. |

All of these are **simulation scenarios** (§9).

---

## 7. Localization & navigation

- **Frames:**
  - The **map frame** has its origin at the dock tag, x pointing out of the dock.
  - The **odom frame** is the one the ESP32 integrates (starts at 0,0,0 on boot or reset).
- **Odometry (ESP32):** distance from the drive encoder, heading from the IMU yaw. The steering angle is **not** used for position (D-005).
- **AprilTag correction (Pi):**
  - A tag layout file lists each tag's ID and pose in the map frame.
  - When a tag is detected, the Pi estimates the camera's pose relative to the tag, then works out the rover's pose using the camera's mounting position on the rover.
  - It then updates the **map←odom offset**, so the ESP32 never needs to be told about corrections.
  - Start with a simple blend (complementary filter); upgrade to an EKF only if needed.
- **Path following:** Pure Pursuit at 20 Hz on the Pi (lookahead ≈ 0.5 m [TBD-test]), within the measured minimum turning radius.
- **Docking:** the route ends with a straight approach of 1–2 m along the dock's axis. The rover steers toward the dock tag by camera, and the funnel rails do the final alignment. Reversing in is optional.
- **Tag family:** tag36h11. Tag size is decided from the laptop test, which measures detection range against tag size.

---

## 8. Data, storage & upload

- **Rover:** SQLite table `samples` with:
  - mission_id, wp_idx, timestamp (ISO 8601, UTC), x, y, pose_age
  - moisture, temp, photo_file, crc32

  Photos are JPEGs (≤ 500 KB) in `photos/<mission_id>/`.
- **Upload (D-006):**
  1. Charge contacts detect power → the rover joins the base's Wi-Fi network.
  2. It sends a manifest (files + CRC32), then the files.
  3. The base checks each CRC32 and replies with an OK list.
  4. The rover marks those files as uploaded and deletes local copies after 30 days.
- **Base:** stores everything and exports CSV (RFC 4180) for spreadsheets. The dashboard shows:
  - the rover's path and sample points on the map
  - photos
  - live status from LoRa
- **Time:** neither Pi has a battery-backed clock (RTC). The rover sets its clock from BEACON time when undocked and over Wi-Fi (NTP via chrony) when docked; see D-013 for the base.

---

## 9. Simulation design (D-011)

- **Protocol code is written without built-in I/O (sans-IO).** It is pure state machines: `on_packet(now, pkt)` and `on_timer(now)` return actions (send this, drive there). The real system runs them with the serial/LoRa transport and the real clock. The simulator runs **the same classes** with a simulated radio and a virtual clock.
- **Channel model:**
  - Airtime from §3.1.
  - Path loss over distance with random shadowing (log-distance model, fitted to range-test data).
  - A packet is received if its SNR ≥ the SF threshold and no other transmission overlaps it. A packet ≥ 6 dB stronger survives the overlap (capture effect).
- **Rover model:** a car-like (bicycle) kinematic model, odometry noise and drift, battery drain per meter and per sample, simulated tag detections within a set range.
- **Scenarios:** normal 3-rover mission · a rover dies mid-mission · a link-loss zone · low battery · a zombie rover returns · base reboots · STOP while the channel is busy.
- **Metrics:**
  - % of waypoints completed
  - time to detect a failure
  - time to reassign
  - stop delay
  - packet delivery ratio
  - airtime used per cycle

---

## 10. Repo layout

Code is grouped by **what it depends on**, not by device, so the simulator can import the
real logic. Lower layers never import higher ones (checked by `uv run lint-imports`).

```
src/aapi/protocol/   ids.py (constants, single source), messages.py (LoRa §4),
                     serial_msgs.py + framing.py (UART §5: COBS + CRC-16/CCITT-FALSE) — stdlib only
src/aapi/core/       sans-IO logic: rover/ (link, mission), base/ (scheduler, fleet), nav/
src/aapi/hal/        interfaces (Clock, EspLink, Camera, TagDetector, Probe) + data types
src/aapi/drivers/    real hardware: esp_serial, camera, AprilTags, SQLite, fake probe
src/aapi/sim/        virtual clock, channel model, rover model, scenarios, metrics
src/aapi/apps/       entry points: aapi-rover, aapi-base
src/aapi/web/        dashboard + upload server
firmware/            PlatformIO (Heltec V3): envs gateway, rover, native (host unit tests);
                     lib/aapi_proto (generated aapi_ids.h + framing), lib/safety, lib/heltec_radio
tools/gen_ids.py     generates the firmware header + golden test vectors from protocol/
config/              per-device TOML (IDs, ports, LoRa/TDMA/nav parameters, tag layout)
tests/               pytest (round-trip, property-based, golden vectors, layer smoke tests)
docs/                architecture.md, decisions.md, proposal, test plans
```

Package manager: uv (`pyproject.toml` + `uv.lock`; extras `rover`/`base`/`sim`, groups
`dev`/`firmware`). See README.md for setup.

---

## 11. Open items
- Pi Zero 2 W or 3B (D-002), camera mount (D-007), probe and sensor (D-008), exact chassis (D-004)
- Final LoRa settings (from the range test) and the FCC approach (D-012)
- Base station clock source (D-013)
- Heltec pin map (once the chassis, motor driver and IMU are chosen)
- Measured values: turning radius, odometry drift per meter, tag detection range, battery drain
