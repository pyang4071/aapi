# AAPI Decision Log

The design decisions for the rover project, with the reasoning and the alternatives considered.
Add new entries at the bottom. If a decision changes, keep the old entry, mark it **Superseded**, and link to the new one.

**Status key:** **Decided** = agreed by the team · **Proposed** = recommended, not yet confirmed · **Open** = still undecided

---

## D-001: Rover microcontroller and LoRa radio
- **Date:** 2026-11-03
- **Status:** Decided
- **Decision:** Heltec WiFi LoRa 32 V3 style board (ESP32-S3 + SX1262 LoRa chip + OLED), **915 MHz version**. ([Amazon listing](https://www.amazon.com/dp/B0DRSJV1ZL))
- **Why:** The LoRa radio is built into the board, so there is no separate LoRa module or SPI wiring. It is cheap (~$20), and the same board can serve as the base station radio and as extra "fake rover" nodes.
- **Alternatives considered:** ESP32-S3 DevKitC + separate SX1262/RFM95W module (the original proposal); more parts and wiring.
- **Notes:**
  - **2 boards ordered (2026-11-03).** That covers the rover and the base station. Buy 1–2 more later for fake-rover testing.
  - Buy 3–4 boards: rover, base station, and 1–2 fake rovers for multi-rover protocol testing.
  - Flash custom firmware (e.g. RadioLib) in place of Meshtastic.
  - Don't use the onboard LiPo charger for the 12 V pack.
  - Free pins are limited; make a pin map before wiring.

## D-002: Rover main computer
- **Date:** 2026-11-03
- **Status:** Open (choosing between Pi Zero 2 W and Pi 3B)
- **Leaning:** Pi Zero 2 W on the rover; a Pi 3B (if available) as the base station.
- **Why:** Both have nearly the same processor and can run AprilTag detection at 640×480. The Zero 2 W is smaller and uses less power, which matters because energy per sample is the top design criterion. The 3B has more RAM and an Ethernet port.
- **Alternatives considered:**
  - Pi 4B/5 (original proposal): more power than needed, higher power draw and cost.
  - Original Pi Zero W: too slow for AprilTags (single core).
- **Notes:** The Pi Zero 2 W needs the narrower camera ribbon cable for the Camera Module 3.

## D-003: Localization
- **Date:** 2026-11-03
- **Status:** Decided
- **Decision:** AprilTags (fixed landmarks in the field plus a tag on the dock), combined with wheel-encoder + IMU odometry. **No GPS.**
- **Why:** Consumer GPS is only accurate to about 3 m, which contradicts the 15 cm navigation target and is too coarse for docking. AprilTags give centimeter-level position corrections and also guide the docking approach.
- **Alternatives considered:**
  - GPS (BN-880, original proposal).
  - GPS + RTK correction: too costly and complex.
- **Notes:** The proposal needs updating: remove GPS from the parts list and change "stop within 3 m" to the AprilTag target.

## D-004: Drivetrain
- **Date:** 2026-11-03
- **Status:** Decided (exact chassis still open)
- **Decision:** Car-style steering: 1 drive motor + 1 steering servo.
- **Why:** The team preference. It is a proven layout, and RC rock-crawler chassis (1/10 scale) fit it well: low speed, high torque, 4-wheel drive from one motor, and they handle soil and uneven ground.
- **Alternatives considered:**
  - Differential drive with 2 motors and 2 encoders: easier odometry, turns in place for docking and probe placement. Not chosen because of the team preference.
  - 4WD skid-steer kit (original proposal): wheel slip makes odometry poor.
- **Trade-offs accepted and how we'll handle them:**
  - Can't turn in place → plan a straight 1–2 m final approach to the dock, with an AprilTag on the dock and funnel guide rails; possibly reverse in.
  - Minimum turning radius → measure it and give it to the path planner.
  - Path following is harder → use the Pure Pursuit algorithm.
  - Can't pivot over an exact probe spot → record the rover's actual position with each reading.
- **Notes:**
  - Use a metal-gear, high-torque steering servo.
  - The motor driver must be able to reverse.
  - RC motors usually lack an encoder, so plan to add one.

## D-005: Odometry sensors
- **Date:** 2026-11-03
- **Status:** Decided
- **Decision:** Distance from an encoder on the drive motor or drivetrain; heading from an IMU gyro (e.g. BNO085). The steering angle is a command only and is **not** used for position.
- **Why:** A hobby servo gives no feedback on its actual angle, and steering slop and wheel slip in soil make the commanded angle unreliable. A gyro gives a heading estimate that doesn't depend on any of that.
- **Alternatives considered:** Using the commanded steering angle in a car-like (bicycle) model; potentiometer feedback on the steering (could be added later).
- **Notes:** The IMU is **required**, not optional. Servos don't provide odometry.

## D-006: Docked data upload
- **Date:** 2026-11-03
- **Status:** Decided
- **Decision:** Upload over Wi-Fi to the base station Pi once docked. The rover knows it is docked when the charging contacts detect power.
- **Why:** Lining up USB data pins reliably through dock contacts is mechanically difficult. Wi-Fi needs no extra connector and still leaves LoRa for commands only.
- **Alternatives considered:** Wired USB 2.0 through the dock (original proposal).
- **Notes:**
  - Update the proposal's "Communicate Data" requirement, which names the USB 2.0 standard.
  - Keep the CRC-32 checksum check on uploaded files.

## D-007: Camera mounting
- **Date:** 2026-11-03
- **Status:** Open
- **Options:**
  1. A pan servo that points the camera forward for tags and sideways for plants. (Recommended: cheapest, and uses the 1 servo in the notes.)
  2. A second camera (USB webcam) for tags. The Pis have only one camera ribbon connector.
  3. A fixed mount angled so the camera can see both plants and tags.
- **Why it matters:** Plant photos need a side/down view, but AprilTag navigation needs a forward view.
- **Notes:**
  - Reserve 2 servo pins on the ESP32 (steering + possible pan).
  - The software treats "point the camera" as a step that does nothing if the camera is fixed.
  - To decide: mount a camera fixed and see whether it can photograph the plant and read a tag from the same pose.

## D-008: Probe actuator and soil sensor
- **Date:** 2026-11-03
- **Status:** Open (deferred)
- **Decision:** Choose later. The software reads the sensor and drives the probe through a generic interface, so other work isn't blocked.
- **Notes:**
  - Lifting the rover is still a risk; measure insertion force before choosing.
  - DS18B20 and RS-485 Modbus probes have different electrical needs (an RS-485 probe needs a transceiver chip and often >5 V).

## D-009: Deliverables (scope)
- **Date:** 2026-11-03
- **Status:** Decided
- **Decision:** As listed in `notes.md`:
  - Simulation with multiple rovers and the communication protocol.
  - Simulation of failures.
  - A real single rover with sensors.
  - Demo: teleoperation, autonomous operation, AprilTag localization, soil probing.
- **Why:** The simulation shows the multi-rover redundancy the farmer asked for without building a whole fleet.
- **Alternatives considered:** A single rover only (original proposal scope).

## D-010: System architecture split
- **Date:** 2026-11-03
- **Status:** Decided (2026-11-03)
- **Decision:**
  - **ESP32:** LoRa radio, motor and servo control, encoder/IMU odometry, and a safety watchdog that stops the motors if the LoRa heartbeat or the Pi's messages stop.
  - **Pi:** protocol and mission logic (Python), AprilTag pose fixes, camera, storage, and Wi-Fi upload. The Pi and ESP32 talk over a serial (UART) link.
- **Why:** Timing-critical and safety jobs stay on the microcontroller. With the protocol in Python, the simulation can run the same protocol code as the real rover.
- **Alternatives considered:** Running the protocol and mission logic in the ESP32 firmware; this would mean maintaining a separate copy of the protocol for the simulation.

## D-011: Simulation tool
- **Date:** 2026-11-03
- **Status:** Decided (2026-11-03)
- **Decision:** A custom Python event-driven simulator that models LoRa transmit time per packet, signal loss with distance, packet collisions, and failures (rover dies, link lost, battery dead, rover stops making progress).
- **Why:** The networks professor warned that NS-3 LoRa simulations take a lot of time to debug. A Python simulator can reuse the real protocol code (see D-010), and its parameters can be checked against the real LoRa range tests.
- **Alternatives considered:** NS-3 with the LoRaWAN module; OMNeT++/FLoRa.

## D-012: LoRa radio settings
- **Date:** 2026-11-03
- **Status:** Proposed
- **Proposal:** Start at SF9 / 500 kHz bandwidth / 4/5 coding rate, using one fixed channel, a private sync word and a network-ID byte. The range test sweeps SF7–SF10 at both 125 and 500 kHz to pick the final setting.
- **Why:** Under FCC §15.247, a single fixed channel at 500 kHz counts as digital modulation, so no frequency hopping is needed. At 125 kHz the rover would have to hop across ≥ 50 channels. 500 kHz also makes packets about 4× shorter: STATUS takes 46 ms instead of 185 ms. The cost is about 6 dB less sensitivity.
- **Alternatives considered:**
  - 125 kHz with frequency hopping (like LoRaWAN US915).
  - 125 kHz on one fixed channel (simplest, but not compliant).
- **Notes:** Keep every packet under 400 ms on air. Confirm the FCC reading with the advisor. Details are in `architecture.md` §3.

## D-013: Clock source for timestamps
- **Date:** 2026-11-03
- **Status:** Open
- **Problem:** Neither Pi has a battery-backed clock (RTC), and the field has no internet for NTP. ISO 8601 timestamps would be wrong after a reboot.
- **Options:**
  1. A DS3231 RTC module (~$5) on the base Pi. The base serves NTP over Wi-Fi and puts the time in each BEACON. (Recommended)
  2. Use a laptop as the base, which has its own clock.
  3. RTC modules on both Pis.
- **Notes:** The rover sets its clock from BEACON time when undocked and over NTP when docked.

## D-014: Remote STOP handled by the ESP32
- **Date:** 2026-11-03
- **Status:** Proposed
- **Proposal:** The ESP32 reads the type byte of every LoRa packet. It acts on STOP itself and watches the BEACON timeout itself, then forwards both to the Pi.
- **Why:** Remote stop still works if the Pi crashes or hangs, and there's no extra delay from relaying through the Pi. This is the only protocol logic in the firmware.
- **Alternatives considered:** The ESP32 forwards everything to the Pi and only acts on Pi commands. Simpler, but stopping would depend on the Pi.

---

## Proposal edits needed
- [ ] Remove the BN-880 GPS from the parts list; add an IMU (D-003, D-005)
- [ ] Change "stop within 3 m" to the AprilTag accuracy target (D-003)
- [ ] Update the drivetrain description and parts list: 4WD kit → car-style chassis (D-004)
- [ ] Update compute: Pi 4B/5 + DevKitC + LoRa module → Heltec V3 + Pi Zero 2 W/3B (D-001, D-002)
- [ ] Change the docked upload: USB 2.0 → Wi-Fi (D-006)
- [ ] Delete the duplicate "Experimental Testing and Evaluation Plan" heading (`proposal.tex:328-329`)
