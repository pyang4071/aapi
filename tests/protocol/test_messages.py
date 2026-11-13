import pytest
from hypothesis import given
from hypothesis import strategies as st

from aapi.protocol.errors import DecodeError, EncodeError
from aapi.protocol.ids import LoraType
from aapi.protocol.messages import (
    HEADER_LEN,
    MAX_TEXT_LEN,
    Packet,
    Resume,
    Stop,
    Text,
    decode_packet,
)

u8 = st.integers(0, 0xFF)
packets = st.one_of(
    st.builds(Text, u8, u8, u8, u8, st.binary(max_size=MAX_TEXT_LEN)),
    st.builds(Stop, u8, u8, u8, u8, u8),
    st.builds(Resume, u8, u8, u8, u8),
)


@given(packets)
def test_round_trip(pkt: Packet) -> None:
    assert decode_packet(pkt.encode()) == pkt


def test_header_byte_order() -> None:
    encoded = Stop(net_id=0xA5, src=0x01, dst=0x03, seq=0x07, reason=0x09).encode()
    assert encoded == bytes([0xA5, LoraType.STOP, 0x01, 0x03, 0x07, 0x09])


@given(st.binary(max_size=40))
def test_decode_only_raises_decode_error(data: bytes) -> None:
    try:
        decode_packet(data)
    except DecodeError:
        pass


def _header(type_: int) -> bytes:
    return bytes([0xA5, type_, 0x00, 0x03, 0x07])


@pytest.mark.parametrize(
    "data",
    [
        bytes(HEADER_LEN - 1),  # too short for a header
        _header(0x01),  # not a LoRa type at all
        _header(LoraType.STATUS),  # real type, no class yet
        _header(LoraType.STOP),  # STOP without its reason byte
        _header(LoraType.STOP) + b"\x01\x02",  # STOP with an extra byte
        _header(LoraType.RESUME) + b"\x01",  # RESUME has no payload
    ],
)
def test_decode_rejects(data: bytes) -> None:
    with pytest.raises(DecodeError):
        decode_packet(data)


@pytest.mark.parametrize(
    "pkt",
    [
        Resume(net_id=0x100, src=0, dst=0, seq=0),
        Resume(net_id=0, src=-1, dst=0, seq=0),
        Resume(net_id=0, src=0, dst=0x100, seq=0),
        Resume(net_id=0, src=0, dst=0, seq=0x100),
        Stop(net_id=0, src=0, dst=0, seq=0, reason=0x100),
        Text(net_id=0, src=0, dst=0, seq=0, text=bytes(MAX_TEXT_LEN + 1)),
    ],
)
def test_encode_rejects_out_of_range(pkt: Packet) -> None:
    with pytest.raises(EncodeError):
        pkt.encode()
