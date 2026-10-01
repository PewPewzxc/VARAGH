"""
PlatformIO pre-build script: compile VARAGH's hot compute paths for speed.

The Arduino core builds everything with -Os (smallest code). That is the right
default for a 6.4 MB app slot, but the few libraries that do the heavy work --
inflating EPUB zips, tokenising XHTML, line breaking and hyphenation, JPEG/PNG
decoding, glyph decompression and drawing, Arabic-script shaping -- run
noticeably faster at -O2. This middleware appends -O2 (the last -O flag wins)
to just those sources, so the size cost stays small.

Deliberately NOT listed: the chapter HTML/CSS parser and section builder,
whose callbacks nest deeply on the 8 KB main-loop stack during background
indexing. -O2 inlines more and can enlarge stack frames there.

Set TIGER_STACK_USAGE=1 in the environment to also emit GCC .su stack-usage
files for every source (used to compare frame sizes against an -Os build).
"""

Import("env")  # noqa: F821 (SCons-injected global)
import os

HOT_PATH_FRAGMENTS = (
    "/lib/expat/",
    "/lib/miniz/",
    "/lib/uzlib/",
    "/lib/InflateReader/",
    "/lib/GfxRenderer/",
    "/lib/EpdFont/",
    "/lib/Utf8/",
    "/lib/MiniBidi/",
    "/lib/Epub/Epub/ParsedText.cpp",
    "/lib/Epub/Epub/hyphenation/",
    "/lib/Epub/Epub/converters/",
    "/libdeps/x4-pro/JPEGDEC/",
    "/libdeps/x4-pro/PNGdec/",
)

SPEED_FLAGS = [] if os.environ.get("TIGER_SPEED_FLAGS") == "0" else ["-O2"]
STACK_USAGE = os.environ.get("TIGER_STACK_USAGE") == "1"


def _is_hot(path):
    normalized = path.replace("\\", "/")
    return any(fragment in normalized for fragment in HOT_PATH_FRAGMENTS)


def tiger_speed_middleware(env, node):
    path = node.srcnode().get_abspath()
    extra = []
    if _is_hot(path):
        extra += SPEED_FLAGS
    if STACK_USAGE:
        extra.append("-fstack-usage")
    if not extra:
        return node
    return env.Object(node, CCFLAGS=env["CCFLAGS"] + extra)


if env.get("PIOENV", "").startswith("x4-pro"):  # noqa: F821
    env.AddBuildMiddleware(tiger_speed_middleware)  # noqa: F821
