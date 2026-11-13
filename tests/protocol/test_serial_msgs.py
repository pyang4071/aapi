import pytest
from hypothesis import given
from hypothesis import strategies as st

from aapi.protocol.framing import LORA_RX_HEADER_LEN, MAX_LORA_PACKET
from aapi.protocol.serial_msgs import DecodeError, EncodeError, LoraRx


@given(
    rssi=st.integers(-0x8000, 0x7FFF),
    snr=st.integers(-0x80, 0x7F),
    packet=st.binary(max_size=MAX_LORA_PACKET),
)
def test_lora_rx_round_trip(rssi: int, snr: int, packet: bytes) -> None:
    msg = LoraRx(rssi, snr, packet)
    encoded = msg.encode()
    assert len(encoded) == LORA_RX_HEADER_LEN + len(packet)
    assert LoraRx.decode(encoded) == msg


@pytest.mark.parametrize(
    "msg",
    [
        LoraRx(0x8000, 0, b""),
        LoraRx(-0x8001, 0, b""),
        LoraRx(0, 0x80, b""),
        LoraRx(0, -0x81, b""),
        LoraRx(0, 0, bytes(MAX_LORA_PACKET + 1)),
    ],
)
def test_lora_rx_encode_rejects_out_of_range(msg: LoraRx) -> None:
    with pytest.raises(EncodeError):
        msg.encode()


@pytest.mark.parametrize(
    "payload",
    [bytes(LORA_RX_HEADER_LEN - 1), bytes(LORA_RX_HEADER_LEN + MAX_LORA_PACKET + 1)],
)
def test_lora_rx_decode_rejects_bad_length(payload: bytes) -> None:
    with pytest.raises(DecodeError):
        LoraRx.decode(payload)
