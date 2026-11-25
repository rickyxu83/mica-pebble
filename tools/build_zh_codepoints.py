#!/usr/bin/env python3
"""Build a compact Chinese codepoint list for Mica's embedded font.

Run with: uv run --with wordfreq python tools/build_zh_codepoints.py
The JSON is committed so normal builds do not need wordfreq or network access.
"""
import json
import re
from pathlib import Path
from wordfreq import iter_wordlist

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "tools" / "zh_codepoints.json"
MAX_FREQUENCY_CHARS = 3300

codepoints = set(range(0x20, 0x7F))
codepoints.update(ord(c) for c in "，。！？；：、（）《》“”‘’…—·°％＋－×÷约年月日时分秒点" )
for path in ROOT.rglob("*"):
    if any(part in {".git", "reference", "build", "node_modules"} for part in path.parts):
        continue
    if not path.is_file() or path.suffix.lower() not in {".c", ".h", ".js", ".json", ".md", ".txt", ".py"}:
        continue
    text = path.read_text(encoding="utf-8", errors="ignore")
    codepoints.update(ord(c) for c in text if ord(c) > 0x7F and not 0xD800 <= ord(c) <= 0xDFFF)

cjk = {cp for cp in codepoints if 0x4E00 <= cp <= 0x9FFF}
for word in iter_wordlist("zh"):
    for char in word:
        cp = ord(char)
        if 0x4E00 <= cp <= 0x9FFF:
            cjk.add(cp)
    if len(cjk) >= MAX_FREQUENCY_CHARS:
        break

codepoints.update(cjk)
codepoints.update((0x25AF, 0x2026))
OUT.write_text(json.dumps({"codepoints": sorted(codepoints)}, ensure_ascii=False, indent=2) + "\n")
print(f"Wrote {len(codepoints)} codepoints ({len(cjk)} CJK) to {OUT}")
