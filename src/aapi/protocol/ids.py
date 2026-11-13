"""ID numbers for message type numbers and addresses.

The 16 codewords: 0x00 1E 27 39 4B 55 6C 72 8D 93 AA B4 C6 D8 E1 FF.
"""

from enum import IntEnum


class SerialType(IntEnum):
    """Byte 0 of a serial frame on the Pi <-> ESP32 cable (§5). Spare: B4 C6 D8 E1 FF."""

    HEARTBEAT = 0x1E
    DRIVE = 0x27
    SERVO = 0x39
    PROBE = 0x4B
    LORA_TX = 0x55
    CONFIG = 0x6C
    ODOM = 0x72
    LORA_RX = 0x8D
    HW_STATUS = 0x93
    STOP_CLEAR = 0xAA  # Pi -> ESP: release the STOP latch (Pi decides, after RESUME)


class LoraType(IntEnum):
    """Byte 1 of a LoRa packet header, over the air (§4). Spare: FF."""

    # base -> rover
    BEACON = 0x1E
    STOP = 0x27
    RESUME = 0x39
    MISSION_LOAD = 0x4B
    WAYPOINTS = 0x55
    MISSION_START = 0x6C
    MISSION_ABORT = 0x72
    RETURN_HOME = 0x8D
    TELEOP = 0x93
    SET_PARAM = 0xAA
    TEXT = 0xB4  # debug chat text, either direction
    # rover -> base
    STATUS = 0xC6
    ACK = 0xD8
    EVENT = 0xE1


class Address(IntEnum):
    """src/dst values in the LoRa header. Rovers are 0x01-0xFE."""

    BASE = 0x00
    BROADCAST = 0xFF
