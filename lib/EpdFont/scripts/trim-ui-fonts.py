"""Drop rarely used glyphs from the built-in menu fonts (inter_*.h) without re-rendering anything.

Every kept glyph keeps its exact bitmap bytes and metrics; only offsets and the interval table change.
Removed: rare Latin (U+01C5-021F except pinyin vowels and Romanian comma letters), rare combining
marks, historic Cyrillic, sub/superscripts, currency signs other than the euro, math operators.
None is used by a menu language. Run again after regenerating the fonts with convert-builtin-fonts.sh.
"""
import re
import sys
from pathlib import Path

DROP = [(0x01C5, 0x01CC), (0x01DD, 0x0217), (0x021C, 0x021F), (0x0346, 0x036F), (0x0460, 0x0489),
        (0x2074, 0x208E), (0x2090, 0x209C), (0x20A0, 0x20AB), (0x20AD, 0x20BF), (0x2202, 0x2298)]
FONTS = ["inter_8_regular", "inter_10_regular", "inter_10_bold", "inter_12_regular", "inter_12_bold"]


def dropped(cp):
    return any(a <= cp <= b for a, b in DROP)


def array(text, name):
    m = re.search(r"(static const \w+ " + name + r"\[\d*\] = \{\n)(.*?)(\};\n)", text, re.S)
    assert m, name
    return m


def trim(path, name):
    text = path.read_text(encoding="utf-8")
    bm = array(text, name + "Bitmaps")
    bitmap = bytes(int(b, 16) for b in re.findall(r"0x([0-9A-Fa-f]{2})", bm.group(2)))
    gm = array(text, name + "Glyphs")
    rows = [(list(map(int, re.findall(r"-?\d+", line.split("}")[0]))), line.split("}", 1)[1])
            for line in gm.group(2).splitlines()]
    im = array(text, name + "Intervals")
    intervals = [tuple(int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)", line)) for line in im.group(2).splitlines()]
    assert sum(last - first + 1 for first, last, _ in intervals) == len(rows)

    out_bitmap = bytearray()
    out_rows, out_intervals = [], []
    for first, last, offset in intervals:
        run_start = None
        for cp in range(first, last + 2):
            keep = cp <= last and not dropped(cp)
            if keep:
                values, tail = rows[offset + cp - first]
                data = bitmap[values[6]:values[6] + values[5]]
                assert len(data) == values[5]
                if run_start is None:
                    run_start = (cp, len(out_rows))
                out_rows.append((values[:6] + [len(out_bitmap) if values[5] else 0], tail, data))
                out_bitmap += data
            elif run_start is not None:
                out_intervals.append((run_start[0], cp - 1, run_start[1]))
                run_start = None
    for values, _, data in out_rows:  # self-check: every kept glyph still points at its own bytes
        assert bytes(out_bitmap[values[6]:values[6] + values[5]]) == data

    lines = [", ".join(f"0x{b:02X}" for b in out_bitmap[i:i + 16]) for i in range(0, len(out_bitmap), 16)]
    new_bitmap = f"static const uint8_t {name}Bitmaps[{len(out_bitmap)}] = {{\n" + "".join(f"    {l},\n" for l in lines) + "};\n"
    new_glyphs = gm.group(1) + "".join("    { " + ", ".join(map(str, v)) + " }" + tail + "\n" for v, tail, _ in out_rows) + gm.group(3)
    new_intervals = im.group(1) + "".join(f"    {{ 0x{a:X}, 0x{b:X}, 0x{o:X} }},\n" for a, b, o in out_intervals) + im.group(3)
    count = re.search(r"(\n    " + name + r"Intervals,\n    )(\d+)(,\n)", text)
    assert count and int(count.group(2)) == len(intervals)
    for old, new in ((bm.group(0), new_bitmap), (gm.group(0), new_glyphs), (im.group(0), new_intervals),
                     (count.group(0), count.group(1) + str(len(out_intervals)) + count.group(3))):
        assert text.count(old) == 1
        text = text.replace(old, new)
    path.write_text(text, encoding="utf-8", newline="\n")
    print(f"{name}: glyphs {len(rows)} -> {len(out_rows)}, bitmap {len(bitmap)} -> {len(out_bitmap)} bytes")


if __name__ == "__main__":
    folder = Path(__file__).resolve().parent.parent / "builtinFonts"
    for font in FONTS:
        trim(folder / f"{font}.h", font)
