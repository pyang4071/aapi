"""Chat demo: type a line, it goes Pi -> ESP32 -> LoRa -> ESP32 -> Pi and is printed there.

Run the same program on both Pis: `uv run aapi-chat --port /dev/ttyUSB0`.
The ESP32s only forward the bytes between 0x00 markers; framing + CRC happen here, end to end.
"""

import argparse
import threading

import serial

from aapi.protocol.framing import FrameDecoder, encode_frame

TEXT = 1  # message type
BAUD = 115200
MAX_TEXT_LEN = 200  # LoRa packet holds 255 bytes


def show(line: str) -> None:
    """Print a line from the reader thread"""
    print(f"Recieved: {line}\n> ", end="", flush=True)


def receive_loop(port: serial.Serial, stop: threading.Event) -> None:
    """Runs in a background thread: read bytes until told to stop, print every message."""
    decoder = FrameDecoder()
    bad_seen = 0
    while not stop.is_set():
        chunk = port.read(256)  # waits at most the port timeout; b"" if nothing came
        for msg_type, payload in decoder.feed(chunk):
            if msg_type == TEXT:
                show(f"< {payload.decode(errors='replace')}")
        if decoder.bad_frames != bad_seen:
            bad_seen = decoder.bad_frames
            show(f"[dropped a corrupted frame, {bad_seen} so far]")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--port", default="/dev/ttyUSB0", help="serial device of the ESP32"
    )
    args = parser.parse_args()

    try:
        port = serial.Serial(args.port, BAUD, timeout=0.1)
    except serial.SerialException as e:
        raise SystemExit(f"can't open {args.port}: {e}") from None

    stop = threading.Event()
    reader = threading.Thread(target=receive_loop, args=(port, stop), daemon=True)
    reader.start()
    print(f"Connected to {args.port}. Send messages here")

    try:
        while True:
            data = input("> ").encode()
            if not data:
                continue
            if len(data) > MAX_TEXT_LEN:
                print(f"too long: {len(data)} bytes (max {MAX_TEXT_LEN})")
                continue
            port.write(encode_frame(TEXT, data))
    except (KeyboardInterrupt, EOFError):  # Ctrl+C or Ctrl+D
        print()
    finally:
        stop.set()
        reader.join(timeout=1)  # returns within one read timeout
        port.close()


if __name__ == "__main__":
    main()
