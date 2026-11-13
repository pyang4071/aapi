"""LoRa packets (architecture.md §4): the 5-byte header plus one class per packet type.

Stdlib only, no I/O. Only the types something uses so far are here; add the rest as chunks need
them (one class + one entry in _CLASSES each).
"""

from dataclasses import dataclass
from typing import ClassVar, Self

from aapi.protocol.errors import DecodeError, EncodeError
from aapi.protocol.framing import MAX_LORA_PACKET
from aapi.protocol.ids import LoraType

HEADER_LEN = 5  # net_id | type | src | dst | seq
MAX_TEXT_LEN = MAX_LORA_PACKET - HEADER_LEN


def _check_u8(name: str, value: int) -> None:
    if not 0 <= value <= 0xFF:
        raise EncodeError(f"{name} out of u8 range: {value}")


@dataclass(frozen=True)
class Header:
    """§4.1. `type` is a plain int: the rover reads it before it knows which class applies."""

    net_id: int
    type: int
    src: int
    dst: int
    seq: int

    def encode(self) -> bytes:
        fields = (self.net_id, self.type, self.src, self.dst, self.seq)
        for name, value in zip(
            ("net_id", "type", "src", "dst", "seq"), fields, strict=True
        ):
            _check_u8(name, value)
        return bytes(fields)

    @classmethod
    def decode(cls, data: bytes) -> "Header":
        """Read the first HEADER_LEN bytes; anything after them is the payload."""
        if len(data) < HEADER_LEN:
            raise DecodeError(f"packet too short for a header: {len(data)} bytes")
        return cls(*data[:HEADER_LEN])


@dataclass(frozen=True)
class Packet:
    """Fields every packet has. The type byte comes from the subclass's TYPE, never a field."""

    TYPE: ClassVar[LoraType]

    net_id: int
    src: int
    dst: int
    seq: int

    def header(self) -> Header:
        return Header(self.net_id, self.TYPE, self.src, self.dst, self.seq)

    def encode(self) -> bytes:
        return self.header().encode() + self._payload()

    def _payload(self) -> bytes:
        raise NotImplementedError

    @classmethod
    def _from_payload(cls, h: Header, payload: bytes) -> Self:
        raise NotImplementedError


@dataclass(frozen=True)
class Text(Packet):
    """Debug chat text, either direction. Not necessarily UTF-8."""

    TYPE = LoraType.TEXT
    text: bytes

    def _payload(self) -> bytes:
        if len(self.text) > MAX_TEXT_LEN:
            raise EncodeError(f"text too long: {len(self.text)} > {MAX_TEXT_LEN}")
        return self.text

    @classmethod
    def _from_payload(cls, h: Header, payload: bytes) -> Self:
        return cls(h.net_id, h.src, h.dst, h.seq, bytes(payload))


@dataclass(frozen=True)
class Stop(Packet):
    """Base -> rover(s): stop now. The rover ESP32 acts on it itself (§5.1)."""

    TYPE = LoraType.STOP
    reason: int

    def _payload(self) -> bytes:
        _check_u8("reason", self.reason)
        return bytes([self.reason])

    @classmethod
    def _from_payload(cls, h: Header, payload: bytes) -> Self:
        if len(payload) != 1:
            raise DecodeError(f"STOP payload must be 1 byte, got {len(payload)}")
        return cls(h.net_id, h.src, h.dst, h.seq, payload[0])


@dataclass(frozen=True)
class Resume(Packet):
    """Base -> rover: leave STOPPED. The rover Pi then sends STOP_CLEAR to its ESP32."""

    TYPE = LoraType.RESUME

    def _payload(self) -> bytes:
        return b""

    @classmethod
    def _from_payload(cls, h: Header, payload: bytes) -> Self:
        if payload:
            raise DecodeError(f"RESUME has no payload, got {len(payload)} bytes")
        return cls(h.net_id, h.src, h.dst, h.seq)


_CLASSES: dict[int, type[Packet]] = {c.TYPE: c for c in (Text, Stop, Resume)}


def decode_packet(data: bytes) -> Packet:
    """Parse one LoRa packet. Raises DecodeError (and nothing else) on bad bytes."""
    h = Header.decode(data)
    cls = _CLASSES.get(h.type)
    if cls is None:
        known = h.type in LoraType.__members__.values()
        raise DecodeError(
            f"{'unsupported' if known else 'unknown'} type 0x{h.type:02X}"
        )
    return cls._from_payload(h, data[HEADER_LEN:])
