#include "heltec_radio.h"

#include <Arduino.h>
#include <RadioLib.h>

#include "serial_msgs.h"

namespace aapi {
namespace {

constexpr uint8_t SYNC_WORD = RADIOLIB_SX126X_SYNC_WORD_PRIVATE;
constexpr float TCXO_VOLTAGE = 1.8;  // Heltec V3 radio clock supply

// Heltec V3 SX1262 pins: NSS, DIO1, RST, BUSY.
SX1262 radio = new Module(8, 14, 12, 13);

// Set from the DIO1 interrupt. DIO1 fires on RX-done *and* TX-done (see radio_send).
volatile bool packet_received = false;

// Interrupt context: only set the flag.
void IRAM_ATTR on_receive() { packet_received = true; }

}  // namespace

bool radio_begin(const RadioConfig& cfg) {
  int16_t state = radio.begin(cfg.freq_mhz, cfg.bw_khz, cfg.sf, cfg.cr, SYNC_WORD, cfg.power_dbm,
                              cfg.preamble, TCXO_VOLTAGE);
  if (state != RADIOLIB_ERR_NONE) {
    return false;  // wrong pins, wrong TCXO voltage, or radio not answering
  }
  radio.setPacketReceivedAction(on_receive);
  return radio.startReceive() == RADIOLIB_ERR_NONE;
}

bool radio_send(const uint8_t* pkt, size_t len) {
  if (len == 0 || len > MAX_LORA_PACKET) {
    return false;  // radio can't send it
  }
  // Blocks until sent (half duplex: packets arriving meanwhile are lost).
  int16_t state = radio.transmit(const_cast<uint8_t*>(pkt), len);
  // DIO1 also fired on TX-done: that flag is not a received packet.
  packet_received = false;
  radio.startReceive();
  return state == RADIOLIB_ERR_NONE;
}

bool radio_poll(RadioPacket& out) {
  if (!packet_received) {
    return false;
  }
  packet_received = false;

  bool ok = false;
  size_t len = radio.getPacketLength();  // must come before readData()
  if (len > 0 && len <= MAX_LORA_PACKET && radio.readData(out.data, len) == RADIOLIB_ERR_NONE) {
    out.len = len;
    out.rssi_dbm = rssi_to_dbm(radio.getRSSI());
    out.snr_qdb = snr_to_qdb(radio.getSNR());
    ok = true;
  }  // else bad length or radio CRC failed: skip it

  radio.startReceive();  // every path: never leave the radio deaf
  return ok;
}

}  // namespace aapi
