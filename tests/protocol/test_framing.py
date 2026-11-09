import pytest
from hypothesis import given
from hypothesis import strategies as st

from aapi.protocol.framing import (
    FrameDecoder,
    FrameError,
    cobs_decode,
    cobs_encode,
    crc16,
    decode_frame,
    encode_frame,
)


def test_crc16_check_value() -> None:
    assert crc16(b"123456789") == 0x29B1


@pytest.mark.parametrize(
    ("data", "encoded"),
    [
        (b"", b"\x01"),
        (b"\x00", b"\x01\x01"),
        (bytes.fromhex("11220033"), bytes.fromhex("0311220233")),
        (bytes.fromhex("03020011"), bytes.fromhex("0303020211")),
        (
            bytes(range(1, 255)),
            b"\xff" + bytes(range(1, 255)),
        ),  # full block, no trailing 01
        (
            bytes(range(1, 255)) + b"\x01",
            b"\xff" + bytes(range(1, 255)) + b"\x02\x01",
        ),  # full block, then more data
    ],
)
def test_cobs_known_vectors(data: bytes, encoded: bytes) -> None:
    assert cobs_encode(data) == encoded
    assert cobs_decode(encoded) == data


@given(st.binary(max_size=1000))
def test_cobs_round_trip_has_no_zeros(data: bytes) -> None:
    encoded = cobs_encode(data)
    assert 0 not in encoded
    assert cobs_decode(encoded) == data


def test_frame_round_trip_and_corruption() -> None:
    frame = encode_frame(5, b"hi\x00there")
    assert frame.endswith(b"\x00") and 0 not in frame[:-1]
    assert decode_frame(frame[:-1]) == (5, b"hi\x00there")

    corrupted = bytearray(frame[:-1])
    corrupted[2] ^= 0x01
    with pytest.raises(FrameError):
        decode_frame(bytes(corrupted))


def test_decoder_resyncs_after_garbage_fed_byte_by_byte() -> None:
    decoder = FrameDecoder()
    stream = b"junk\x00" + encode_frame(7, b"ok")
    frames = []
    for byte in stream:
        frames += decoder.feed(bytes([byte]))
    assert frames == [(7, b"ok")]


def test_decoder_drops_only_the_bad_frame() -> None:
    bad = bytearray(encode_frame(2, b"bad"))
    bad[1] ^= 0x01
    decoder = FrameDecoder()
    frames = decoder.feed(encode_frame(1, b"a") + bytes(bad) + encode_frame(3, b"c"))
    assert frames == [(1, b"a"), (3, b"c")]
    assert decoder.bad_frames == 1
