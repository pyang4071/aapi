// Gateway (base side): USB serial <-> LoRa for the base Pi. No decisions (architecture.md §5.1).
//   Pi -> air: decode serial frame; LORA_TX -> transmit its payload (the raw LoRa packet).
//   air -> Pi: received packet + RSSI/SNR -> LORA_RX frame.

#include <Arduino.h>
#include <RadioLib.h>

#include "aapi_ids.h"
#include "framing.h"
#include "serial_msgs.h"

constexpr unsigned long BAUD = 460800;
// radio.transmit() blocks for tens of ms; hold a few full frames meanwhile.
constexpr size_t SERIAL_RX_BUFFER = 4 * aapi::MAX_FRAME_LEN;
constexpr float FREQ_MHZ = 915.0;

// LoRa settings
constexpr float BANDWIDTH_KHZ = 500.0;
constexpr uint8_t SPREADING_FACTOR = 9;
constexpr uint8_t CODING_RATE = 5;  // RadioLib writes 4/5 as 5
constexpr uint8_t SYNC_WORD = RADIOLIB_SX126X_SYNC_WORD_PRIVATE;
constexpr int8_t POWER_DBM = 14;
constexpr uint16_t PREAMBLE_LEN = 8;
constexpr float TCXO_VOLTAGE = 1.8;  // Heltec V3 radio clock supply

constexpr int LED_PIN = 35;

// Heltec V3 SX1262 pins: NSS, DIO1, RST, BUSY.
SX1262 radio = new Module(8, 14, 12, 13);

aapi::FrameDecoder decoder;

volatile bool packet_received = false;

// Called from an interrupt when a LoRa packet arrives: only set a flag
void IRAM_ATTR on_receive() { packet_received = true; }

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

  int16_t state = radio.begin(FREQ_MHZ, BANDWIDTH_KHZ, SPREADING_FACTOR, CODING_RATE, SYNC_WORD,
                              POWER_DBM, PREAMBLE_LEN, TCXO_VOLTAGE);
  if (state != RADIOLIB_ERR_NONE) {
    fail_forever();  // wrong pins, wrong TCXO voltage, or radio not answering
  }

  radio.setPacketReceivedAction(on_receive);
  if (radio.startReceive() != RADIOLIB_ERR_NONE) {
    fail_forever();
  }
}

// A good frame came in over serial. payload() is only valid until the next feed().
void handle_frame() {
  if (decoder.type() != aapi::serial::LORA_TX) {
    return;  // gateway has no motors or sensors: other types are ignored
  }
  size_t len = decoder.payload_len();
  if (len == 0 || len > aapi::MAX_LORA_PACKET) {
    return;  // radio can't send it
  }
  // transmit() blocks until sent (half duplex: packets arriving meanwhile are lost)
  radio.transmit(const_cast<uint8_t*>(decoder.payload()), len);
  packet_received = false;
  radio.startReceive();
}

// A LoRa packet arrived: send it to the Pi with its signal readings.
void forward_packet() {
  uint8_t pkt[aapi::MAX_LORA_PACKET];
  size_t len = radio.getPacketLength();  // called before readData()
  if (len == 0 || len > aapi::MAX_LORA_PACKET) {
    return;
  }
  if (radio.readData(pkt, len) != RADIOLIB_ERR_NONE) {
    return;  // radio CRC failed: skip it
  }

  uint8_t payload[aapi::MAX_PAYLOAD_LEN];
  size_t n = aapi::encode_lora_rx(aapi::rssi_to_dbm(radio.getRSSI()),
                                  aapi::snr_to_qdb(radio.getSNR()), pkt, len, payload,
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
  if (packet_received) {
    packet_received = false;
    forward_packet();
    radio.startReceive();
  }
}
