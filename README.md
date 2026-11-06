# AAPI — Autonomous Agricultural Probing Interface

Rover + base station over LoRa, with a multi-rover simulator:

## Laptop setup

```sh
uv sync --all-extras                 # Python 3.13 + everything
uv run pytest                        # protocol, core, smoke tests
```

## Raspberry Pi setup (64-bit Raspberry Pi OS Lite)

```sh
curl -LsSf https://astral.sh/uv/install.sh | sh
sudo apt install python3-picamera2               # rover only; not on PyPI
```

## Blink LED
To complile and upload:
```
uv run --group firmware pio run -d firmware -e blink -t upload
```

To monitor:
```
uv run --group firmware pio device monitor -b 115200
```
