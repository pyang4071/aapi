from enum import IntEnum
from itertools import combinations

import pytest

from aapi.protocol.ids import LoraType, SerialType

MIN_DISTANCE = 4  # a 1-3 bit error can't turn one valid type into another


@pytest.mark.parametrize("enum", [SerialType, LoraType])
def test_codes_are_valid(enum: type[IntEnum]) -> None:
    # __members__ includes aliases: a repeated value silently becomes an alias, and
    # iterating the enum would skip it.
    values = [member.value for member in enum.__members__.values()]
    assert len(values) == len(set(values)), "duplicate value"
    assert all(0 < v <= 0xFF for v in values), "must be one byte, never 0x00"
    for a, b in combinations(values, 2):
        assert (a ^ b).bit_count() >= MIN_DISTANCE, f"0x{a:02X} vs 0x{b:02X}"
