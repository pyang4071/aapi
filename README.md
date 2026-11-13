# AAPI — Autonomous Agricultural Probing Interface

Rover + base station over LoRa, with a multi-rover simulator:

## Environment setup 

### Laptop setup

```sh
uv sync --all-extras                 # Python 3.13 + everything
uv run pytest                        # protocol, core, smoke tests
```

### Raspberry Pi setup (64-bit Raspberry Pi OS Lite)

```sh
curl -LsSf https://astral.sh/uv/install.sh | sh
sudo apt install python3-picamera2               # rover only; not on PyPI
```

## Communication Tests 

### Blink LED
To complile and upload:
```
uv run --group firmware pio run -d firmware -e blink -t upload
```

To monitor:
```
uv run --group firmware pio device monitor -b 115200
```

### Chat demo (two Pi + ESP32 pairs, over LoRa)
Flash the gateway firmware onto each ESP32 (from its Pi):
```
uv run --group firmware pio run -d firmware -e gateway -t upload
```

Then on each Pi:
```
uv run aapi-chat --port /dev/ttyUSB0
```
Type a line and it appears on the other Pi. Close any `pio device monitor` first: only one program can hold the port.

## Codebase stuff

### Generate framing and id in cpp

Uses the same framing and id on both the firmware and rpi so we generate the cpp version via the python version. Generates to 
`./firmware/lib/aapi-proto/`. To generate, run
```
uv run python tools/gen_ids.py
```

### Tests 

To run the python tests 
```
uv run pytest
```

To run the the firnware tests 
```
uv run --group firmware pio test -d firmware -e native
```

