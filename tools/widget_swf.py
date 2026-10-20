r"""Writes the empty movies (31-byte AS2 SWF, one blank frame) of the invisible menus Immersive HUD SKSE fades, one per
gauge group, to res\core\Interface\TemperatureGauge. Immersive HUD names its MCM entries after the file names.

usage: python widget_swf.py"""

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NAMES = ["TemperatureGauge", "TemperatureGaugeNeeds", "TemperatureGaugeStress"]


def rect(x0: int, x1: int, y0: int, y1: int, bits: int = 16) -> bytes:
    value = bits
    for v in (x0, x1, y0, y1):
        value = (value << bits) | (v & ((1 << bits) - 1))
    total = 5 + 4 * bits
    pad = (8 - total % 8) % 8
    return (value << pad).to_bytes((total + pad) // 8, "big")


def tag(code: int, body: bytes = b"") -> bytes:
    return struct.pack("<H", (code << 6) | len(body)) + body


def movie() -> bytes:
    body = rect(0, 1280 * 20, 0, 720 * 20) + struct.pack("<HH", 30 << 8, 1)
    body += tag(69, struct.pack("<I", 0))  # FileAttributes: AS2, no network access
    body += tag(1)  # ShowFrame
    body += tag(0)  # End
    return b"FWS" + bytes([8]) + struct.pack("<I", 8 + len(body)) + body


def main():
    folder = ROOT / "res" / "core" / "Interface" / "TemperatureGauge"
    folder.mkdir(parents=True, exist_ok=True)
    data = movie()
    for name in NAMES:
        (folder / f"{name}.swf").write_bytes(data)
    print(f"wrote {len(NAMES)} movies of {len(data)} bytes to {folder}")


if __name__ == "__main__":
    main()
