"""Serial-link message payloads"""

from dataclasses import dataclass

from aapi.protocol.framing import LORA_RX_HEADER_LEN, MAX_LORA_PACKET


class EncodeError(Exception):
    """A field is out of range for its wire format."""


class DecodeError(Exception):
    """Payload bytes don't form a valid message."""


@dataclass(frozen=True)
class LoraRx:
    """ESP32 → Pi: one LoRa packet as received. Wire: rssi i16 | snr i8 | packet."""

    rssi_dbm: int  # signal strength, dBm
    snr_qdb: int  # signal-to-noise ratio in 0.25 dB steps (the SX1262's resolution)
    packet: bytes  # the raw bytes that came over the air

    @property
    def snr_db(self) -> float:
        return self.snr_qdb / 4

    def encode(self) -> bytes:
        if not -0x8000 <= self.rssi_dbm <= 0x7FFF:
            raise EncodeError(f"rssi_dbm out of i16 range: {self.rssi_dbm}")
        if not -0x80 <= self.snr_qdb <= 0x7F:
            raise EncodeError(f"snr_qdb out of i8 range: {self.snr_qdb}")
        if len(self.packet) > MAX_LORA_PACKET:
            raise EncodeError(
                f"packet too long: {len(self.packet)} > {MAX_LORA_PACKET}"
            )
        return (
            self.rssi_dbm.to_bytes(2, "little", signed=True)
            + self.snr_qdb.to_bytes(1, "little", signed=True)
            + self.packet
        )

    @classmethod
    def decode(cls, payload: bytes) -> "LoraRx":
        if len(payload) < LORA_RX_HEADER_LEN:
            raise DecodeError(f"LORA_RX too short: {len(payload)} bytes")
        if len(payload) > LORA_RX_HEADER_LEN + MAX_LORA_PACKET:
            raise DecodeError(f"LORA_RX too long: {len(payload)} bytes")
        return cls(
            rssi_dbm=int.from_bytes(payload[0:2], "little", signed=True),
            snr_qdb=int.from_bytes(payload[2:3], "little", signed=True),
            packet=bytes(payload[LORA_RX_HEADER_LEN:]),
        )
