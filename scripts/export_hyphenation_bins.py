"""Write the card files for CROSSINK_HYPHENATION_SD builds: hyph-<tag>.bin = u32 LE root offset + trie.

Usage: python scripts/export_hyphenation_bins.py <output folder>   (copy it to /.crosspoint/hyphenation)
"""
import re
import struct
import sys
from pathlib import Path

TAGS = ("fr", "ru", "es", "sv", "uk", "pl")
generated = Path(__file__).resolve().parent.parent / "lib/Epub/Epub/hyphenation/generated"
out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for tag in TAGS:
    text = (generated / f"hyph-{tag}.trie.h").read_text()
    body = text[text.index("_trie_data[] = {"):text.index("};")]
    data = bytes(int(b, 16) for b in re.findall(r"0x([0-9A-Fa-f]{2})\b", body))
    root = int(re.search(rf"{tag}_patterns = \{{\s*0x([0-9A-Fa-f]+)u", text).group(1), 16)
    assert root < len(data), tag
    (out / f"hyph-{tag}.bin").write_bytes(struct.pack("<I", root) + data)
    print(f"hyph-{tag}.bin {len(data) + 4} bytes")
