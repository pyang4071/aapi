// Rover ESP32: LoRa <-> UART for the rover Pi, plus the STOP latch

#include <Arduino.h>

#include "aapi_ids.h"
#include "framing.h"
#include "heltec_radio.h"
#include "lora_header.h"
#include "serial_msgs.h"
#include "stop_latch.h"

constexpr unsigned long BAUD = 460800;
constexpr size_t SERIAL_RX_BUFFER = 4 * aapi::MAX_FRAME_LEN;

constexpr int LED_PIN = 35;  // steady on = STOP latched; fast blink = radio failure

aapi::FrameDecoder decoder;
aapi::Identity identity;  // any STOP counts
aapi::StopLatch latch;

// fast blink => error
void fail_forever() {
  pinMode(LED_PIN, OUTPUT);
  while (true) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(100);
  }
}

void stop_motors() {
}

void setup() {
  Serial.setRxBufferSize(SERIAL_RX_BUFFER); 
  Serial.begin(BAUD);
  pinMode(LED_PIN, OUTPUT);

  if (!aapi::radio_begin()) {
    fail_forever();  // wrong pins, wrong TCXO voltage, or radio not answering
  }
}

// A good frame came in over serial. payload() is only valid until the next feed().
void handle_frame() {
  switch (decoder.type()) {
    case aapi::serial::LORA_TX:
      aapi::radio_send(decoder.payload(), decoder.payload_len());  // rejects lengths it can't send
      break;
    case aapi::serial::STOP_CLEAR:
      latch.clear();
      break;
    default:
      break;  // DRIVE, CONFIG, HEARTBEAT, ...
  }
}

// Send a received LoRa packet to the Pi with its signal readings.
void forward_packet(const aapi::RadioPacket& pkt) {
  uint8_t payload[aapi::MAX_PAYLOAD_LEN];
  size_t n = aapi::encode_lora_rx(pkt.rssi_dbm, pkt.snr_qdb, pkt.data, pkt.len, payload,
                                  sizeof(payload));
  uint8_t frame[aapi::MAX_FRAME_LEN];
  size_t m = aapi::encode_frame(aapi::serial::LORA_RX, payload, n, frame, sizeof(frame));
  if (n > 0 && m > 0) {
    Serial.write(frame, m);
  }
}

// A LoRa packet arrived: decide from the header alone
void handle_packet(const aapi::RadioPacket& pkt) {
  aapi::LoraHeader h;
  if (!aapi::parse_header(pkt.data, pkt.len, &h)) {
    return; 
  }

  switch (aapi::classify(h, identity)) {
    case aapi::Action::DROP:
      return;  // not our network
    case aapi::Action::STOP:
      latch.trip(0);
      stop_motors();
      break;
    case aapi::Action::FORWARD:
      break;
  }
  forward_packet(pkt);
}

void loop() {
  // --- serial -> LoRa ---
  while (Serial.available() > 0) {
    if (decoder.feed(static_cast<uint8_t>(Serial.read()))) {
      handle_frame();
    }
  }

  // --- LoRa -> serial ---
  static aapi::RadioPacket pkt;  // static: 260 B off the loop stack
  if (aapi::radio_poll(pkt)) {
    handle_packet(pkt);
  }

  digitalWrite(LED_PIN, latch.active() ? HIGH : LOW);
}
