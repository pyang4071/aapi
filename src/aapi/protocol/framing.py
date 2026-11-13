"""Serial-link framing (architecture.md §5): COBS + CRC-16/CCITT-FALSE. Stdlib only, no I/O."""

import binascii

CRC_len = 2
MAX_LORA_PACKET = 255  # SX1262 length register is one byte
LORA_RX_HEADER_LEN = 3  # rssi i16 + snr i8, in front of the packet
MAX_PAYLOAD_LEN = LORA_RX_HEADER_LEN + MAX_LORA_PACKET  # biggest payload
MAX_BODY_LEN = 1 + MAX_PAYLOAD_LEN + CRC_len  # type + payload + crc
MAX_ENCODED_LEN = (
    MAX_BODY_LEN + MAX_BODY_LEN // 254 + 1
)  # add one ending byte and at most 1 byte per 254 for removign the 0x00 in message


class FrameError(Exception):
    """Frame had bad COBS encoding, was too short, or failed its CRC."""


def crc16(data: bytes) -> int:
    return binascii.crc_hqx(data, 0xFFFF)


def cobs_encode(data: bytes) -> bytes:
    out = bytearray()
    block = bytearray()  # non-zero bytes since the last zero
    for i, byte in enumerate(data):
        if byte == 0:
            out.append(len(block) + 1)  # count byte stands in for the zero
            out += block
            block.clear()
        else:
            block.append(byte)
            if len(block) == 254:  # a count byte can't go above 0xFF
                out.append(0xFF)  # 0xFF means "254 bytes, NO zero after"
                out += block
                block.clear()
                if i == len(data) - 1:
                    return bytes(out)  # data ended exactly on a full block
    out.append(len(block) + 1)  # last block (no zero after it)
    out += block
    return bytes(out)


def cobs_decode(data: bytes) -> bytes:
    out = bytearray()
    i = 0
    while i < len(data):
        code = data[i]
        if code == 0:
            raise FrameError("zero byte inside COBS data")
        end = i + code
        if end > len(data):
            raise FrameError("COBS block runs past end of frame")
        block = data[i + 1 : end]
        if 0 in block:
            raise FrameError("zero byte inside COBS data")
        out += block
        i = end
        if code != 0xFF and i < len(
            data
        ):  # put the zero back (not after the last block)
            out.append(0)
    return bytes(out)


def encode_frame(msg_type: int, payload: bytes = b"") -> bytes:
    """Build the bytes to write to the port: type + payload + CRC, COBS-encoded, then 0x00."""
    if not 0 <= msg_type <= 0xFF:
        raise ValueError(f"msg_type out of range: {msg_type}")
    if len(payload) > MAX_PAYLOAD_LEN:
        raise ValueError(f"payload too long: {len(payload)} > {MAX_PAYLOAD_LEN}")
    body = bytes([msg_type]) + payload
    body += crc16(body).to_bytes(CRC_len, "little")
    return cobs_encode(body) + b"\x00"


def decode_frame(frame: bytes) -> tuple[int, bytes]:
    """Decode one frame (0x00 marker already removed) into (type, payload)."""
    body = cobs_decode(frame)
    if len(body) < 1 + CRC_len:
        raise FrameError("frame too short")
    received = int.from_bytes(body[-CRC_len:], "little")
    if crc16(body[:-CRC_len]) != received:
        raise FrameError("CRC mismatch")
    return body[0], body[1:-CRC_len]


class FrameDecoder:
    """Splits a byte stream into frames; keeps partial frames between feed() calls."""

    def __init__(self) -> None:
        self._buf = bytearray()
        self._discarding = False  # True after an overlong frame, until the next 0x00
        self.bad_frames = 0

    def feed(self, chunk: bytes) -> list[tuple[int, bytes]]:
        frames: list[tuple[int, bytes]] = []
        for byte in chunk:
            if byte == 0:  # end of a frame
                if self._buf and not self._discarding:
                    try:
                        frames.append(decode_frame(bytes(self._buf)))
                    except FrameError:
                        self.bad_frames += 1  # drop it, keep going
                self._buf.clear()
                self._discarding = False
            elif not self._discarding:
                self._buf.append(byte)
                if len(self._buf) > MAX_ENCODED_LEN:
                    self._buf.clear()  # garbage with no 0x00: give up on it
                    self._discarding = True
                    self.bad_frames += 1
        return frames
