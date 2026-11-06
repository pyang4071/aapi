"""AAPI: Autonomous Agricultural Probing Interface.

Layers (lower layers never import higher ones; enforced by `uv run lint-imports`):

    apps                      entry points that wire everything together
    drivers | sim | web       real hardware / simulated hardware / dashboard
    core                      sans-IO state machines and algorithms
    hal                       interfaces + plain data types
    protocol                  wire formats and constants
"""
