"""Refuse builds that would silently produce the wrong firmware.

1. Library links: .pio/libdeps/<env>/*.pio-link must point into THIS checkout. A copied .pio folder
   keeps the old checkout's path, so edits to this checkout's freeink-sdk are ignored without a word.
2. One build at a time: two builds in one checkout overwrite each other's object files and the
   generated I18n headers; the link then fails or, worse, mixes them.
"""
import atexit
import ctypes
import json
import os
import sys
from pathlib import Path

Import("env")  # noqa: F821  (SCons)

project = Path(env["PROJECT_DIR"]).resolve()  # noqa: F821


def fail(message):
    sys.stderr.write("\n*** build_guard: " + message + "\n\n")
    env.Exit(1)  # noqa: F821


for link in (project / ".pio" / "libdeps").glob("*/*.pio-link"):
    try:
        cwd = Path(json.loads(link.read_text(encoding="utf-8"))["cwd"]).resolve()
    except (OSError, ValueError, KeyError):
        continue
    if cwd != project:
        fail(f"{link.relative_to(project)} points at {cwd}, not this checkout.\n"
             f"    Fix: delete .pio/libdeps (it is re-created) or correct the path in the .pio-link files.")


def alive(pid):
    if os.name == "nt":
        handle = ctypes.windll.kernel32.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
        if not handle:
            return False
        code = ctypes.c_ulong()
        ctypes.windll.kernel32.GetExitCodeProcess(handle, ctypes.byref(code))
        ctypes.windll.kernel32.CloseHandle(handle)
        return code.value == 259  # STILL_ACTIVE
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    return True


lock = project / ".pio" / "build.lock"
lock.parent.mkdir(exist_ok=True)
try:
    other = int(lock.read_text().strip())
except (OSError, ValueError):
    other = 0
# The ESP32-C3 build runs in two stages: the first starts a second pio inside itself. That child
# inherits GUARD_ENV naming the lock holder and shares its lock instead of being refused.
GUARD_ENV = "VARAGH_BUILD_GUARD_PID"
nested = other != 0 and os.environ.get(GUARD_ENV) == str(other)
if other and other != os.getpid() and not nested and alive(other):
    fail(f"another build is already running in this checkout (process {other}). Wait for it or stop it first.")
if not nested:
    lock.write_text(str(os.getpid()))
    os.environ[GUARD_ENV] = str(os.getpid())


def release():
    try:
        if lock.read_text().strip() == str(os.getpid()):
            lock.unlink()
    except OSError:
        pass


atexit.register(release)
