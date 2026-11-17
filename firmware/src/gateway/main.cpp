// Gateway (base side): USB serial <-> LoRa for the base Pi.
//   Pi -> air: decode serial frame; LORA_TX -> transmit its payload (the raw LoRa packet).
//   air -> Pi: received packet + RSSI/SNR -> LORA_RX frame.

#include <Arduino.h>

#include "aapi_ids.h"
#include "framing.h"
#include "heltec_radio.h"
#include "serial_msgs.h"

constexpr unsigned long BAUD = 460800;
// radio_send() blocks for tens of ms; hold a few full frames meanwhile.
constexpr size_t SERIAL_RX_BUFFER = 4 * aapi::MAX_FRAME_LEN;

constexpr int LED_PIN = 35;

aapi::FrameDecoder decoder;

// fast blink => error
void fail_forever() {
  pinMode(LED_PIN, OUTPUT);
  while (true) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(100);
  }
}

void setup() {
  Serial.setRxBufferSize(SERIAL_RX_BUFFER);  // must come before begin()
  Serial.begin(BAUD);

  if (!aapi::radio_begin()) {
    fail_forever();  // wrong pins, wrong TCXO voltage, or radio not answering
  }
}

// A good frame came in over serial. payload() is only valid until the next feed().
void handle_frame() {
  if (decoder.type() != aapi::serial::LORA_TX) {
    return;  // gateway has no motors or sensors: other types are ignored
  }
  aapi::radio_send(decoder.payload(), decoder.payload_len());  // rejects lengths it can't send
}

// A LoRa packet arrived: send it to the Pi with its signal readings.
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
    forward_packet(pkt);
  }
}
