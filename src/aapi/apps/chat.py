"""Chat demo: type a line, it goes Pi -> ESP32 -> LoRa -> ESP32 -> Pi and is printed there.

Run the same program on both Pis: `uv run aapi-chat --port /dev/ttyUSB0`.
Each ESP32 gateway decodes the serial frame and transmits only the text (LORA_TX); the
receiving gateway sends it up as LORA_RX with RSSI/SNR.
"""

import argparse
import readline
import threading

import serial

from aapi.protocol import ids
from aapi.protocol.framing import MAX_LORA_PACKET, FrameDecoder, encode_frame
from aapi.protocol.serial_msgs import DecodeError, LoraRx

BAUD = 460800  # must match the gateway firmware
MAX_TEXT_LEN = MAX_LORA_PACKET  # the text is the whole LoRa packet


def show(line: str) -> None:
    """Print a line from the reader thread"""
    typed = readline.get_line_buffer()
    print(f"\r\033[K Recieved: {line}\n> {typed}", end="", flush=True)


def receive_loop(port: serial.Serial, stop: threading.Event) -> None:
    """Runs in a background thread: read bytes until told to stop, print every message."""
    decoder = FrameDecoder()
    bad_seen = 0
    while not stop.is_set():
        chunk = port.read(256)  # waits at most the port timeout; b"" if nothing came
        for msg_type, payload in decoder.feed(chunk):
            if msg_type != ids.LORA_RX:
                continue
            try:
                rx = LoraRx.decode(payload)
            except DecodeError:
                continue
            text = rx.packet.decode(errors="replace")
            show(f"{text}  (RSSI {rx.rssi_dbm} dBm, SNR {rx.snr_db:.2f} dB)")
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
            port.write(encode_frame(ids.LORA_TX, data))
    except (KeyboardInterrupt, EOFError):  # Ctrl+C or Ctrl+D
        print()
    finally:
        stop.set()
        reader.join(timeout=1)  # returns within one read timeout
        port.close()


if __name__ == "__main__":
    main()
