#!/usr/bin/env python3
"""Generate Mica's compact Noto CJK subset in Pebble PBF v3 format."""
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SDK_FONT_TOOLS = Path.home() / ".local/share/pebble-sdk/SDKs/current/sdk-core/pebble/common/tools"
TTF = Path("/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc")
OUT = ROOT / "app/resources/fonts/MICA_ZH_18.pbf"

import sys
sys.path.insert(0, str(SDK_FONT_TOOLS))
from font.fontgen import Font  # noqa: E402


def main() -> None:
    font = Font(str(TTF), 18, 32700, 2048, False)
    font.set_codepoint_list(str(ROOT / "tools/zh_codepoints.json"))
    font.set_compression("RLE4")
    font.build_tables()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(font.bitstring())
    print(f"Wrote {OUT} ({OUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
