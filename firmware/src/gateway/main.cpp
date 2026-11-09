// Chat demo forwarder: USB serial <-> LoRa. Same firmware on both Heltec V3 boards.
// ESP32 only forwards; no decryption

#include <Arduino.h>
#include <RadioLib.h>

constexpr unsigned long BAUD = 115200;
constexpr float FREQ_MHZ = 915.0;
constexpr size_t MAX_PACKET = 255;  // SX1262 packet limit

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

uint8_t serial_buf[MAX_PACKET];
size_t serial_len = 0;
bool discarding = false;  // true after an overlong frame, until the next 0x00

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

// complete frame came in over serial (send as LoRa packet)
void send_frame() {
  radio.transmit(serial_buf, serial_len);  // blocks until sent (half duplex: no receiving now)
  packet_received = false;
  radio.startReceive();
}

void loop() {
  // --- serial -> LoRa ---
  while (Serial.available() > 0) {
    uint8_t b = Serial.read();
    if (b == 0x00) {  // end of a frame
      if (serial_len > 0 && !discarding) {
        send_frame();
      }
      serial_len = 0;
      discarding = false;
    } else if (discarding) {
      // drop bytes until the next 0x00
    } else if (serial_len < MAX_PACKET) {
      serial_buf[serial_len++] = b;
    } else {
      discarding = true;  // too long for one LoRa packet: drop this whole frame
    }
  }

  // --- LoRa -> serial ---
  if (packet_received) {
    packet_received = false;
    uint8_t pkt[MAX_PACKET];
    size_t len = radio.getPacketLength();  // called before readData()
    int16_t state = radio.readData(pkt, len);
    if (state == RADIOLIB_ERR_NONE && len > 0) {  // radio CRC failed -> skip it
      Serial.write(pkt, len);
      Serial.write(static_cast<uint8_t>(0x00));  // put back the end-of-frame marker
    }
    radio.startReceive();
  }
}
