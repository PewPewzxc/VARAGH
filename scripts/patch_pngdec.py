"""
PlatformIO pre-build script: make PNGdec's bundled zlib copy match data byte by byte.

PNGdec 1.1.6 defines ALLOWS_UNALIGNED unconditionally in its zlib inflate.c and
inffast.c and then copies back-references four bytes at a time. In inffast.c a
match that starts in the history window and continues from the fresh output is
copied as whole words even when source and destination are only 1-3 bytes
apart, so it reads bytes that have not been written yet; several branches also
write up to three bytes past the end of the copy. Some PNGs therefore decode
with the lower part of the image corrupted (found by comparing PNGdec with
Pillow on photo PNGs: a 480x800 RGBA image went wrong from row 618 on).

Commenting out the define selects zlib's standard byte-copy loops, which cost a
few percent of PNG decode time. The edit is idempotent and applied to every
environment's PNGdec copy before it is compiled.
"""

Import("env")  # noqa: F821 (SCons-injected global)
import os
import re
import sys

TARGETS = ("inflate.c", "inffast.c")
DISABLED = "// #define ALLOWS_UNALIGNED  /* VARAGH: byte-wise copies, see scripts/patch_pngdec.py */"
DEFINE_RE = re.compile(r"^#define ALLOWS_UNALIGNED[ \t]*(?=\r?$)", re.MULTILINE)


def patch_pngdec(env):
    libdeps_dir = os.path.join(env["PROJECT_DIR"], ".pio", "libdeps")
    if not os.path.isdir(libdeps_dir):
        return
    for env_dir in os.listdir(libdeps_dir):
        src_dir = os.path.join(libdeps_dir, env_dir, "PNGdec", "src")
        if not os.path.isdir(src_dir):
            continue
        for name in TARGETS:
            path = os.path.join(src_dir, name)
            with open(path, "r", encoding="latin-1", newline="") as f:
                text = f.read()
            if DISABLED in text:
                continue
            patched, count = DEFINE_RE.subn(DISABLED, text, count=1)
            if count != 1:
                sys.stderr.write(
                    "ERROR: PNGdec %s has no '#define ALLOWS_UNALIGNED' line to disable; "
                    "re-check scripts/patch_pngdec.py against this PNGdec version\n" % path
                )
                raise SystemExit(1)
            with open(path, "w", encoding="latin-1", newline="") as f:
                f.write(patched)
            print("Patched PNGdec %s (%s): byte-wise match copies" % (name, env_dir))


patch_pngdec(env)  # noqa: F821
