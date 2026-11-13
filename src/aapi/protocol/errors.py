"""Errors shared by every message module in protocol/."""


class EncodeError(Exception):
    """A field is out of range for its wire format."""


class DecodeError(Exception):
    """Bytes don't form a valid message."""
