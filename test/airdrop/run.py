"""Run the AirDrop host checks, including sanitizer coverage, without device hardware.

AIRDROP_NO_SANITIZE=1 drops sanitizers; otherwise the best set the compiler links is used
(ASan+UBSan, else trapping UBSan, e.g. on MinGW). The chosen flags reach the Python
tests through AIRDROP_CFLAGS, and the compilers through CC/CXX.
"""

from pathlib import Path
import os
import shlex
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
WARN = ["-Wall", "-Wextra", "-fstack-protector-all"]


def pick_flags(cc, directory):
    if os.environ.get("AIRDROP_NO_SANITIZE") == "1":
        return []
    probe = Path(directory) / "probe.c"
    probe.write_text("int main(void){return 0;}\n")
    for flags in (["-fsanitize=address,undefined", "-fno-omit-frame-pointer"],
                  ["-fsanitize=undefined", "-fsanitize-trap=undefined"]):
        exe = Path(directory) / "probe"
        if subprocess.run(cc + flags + [str(probe), "-o", str(exe)], capture_output=True).returncode == 0:
            return flags
    return []


def main():
    os.environ.setdefault("CC", "cc" if shutil.which("cc") else "gcc")
    os.environ.setdefault("CXX", "c++")
    with tempfile.TemporaryDirectory(prefix="crosspoint-airdrop-") as directory:
        flags = pick_flags(shlex.split(os.environ["CC"]), directory) + WARN
        os.environ["AIRDROP_CFLAGS"] = " ".join(flags)
        print("AirDrop host tests with", os.environ["AIRDROP_CFLAGS"])
        for name in ("serve_test.c", "stream_test.cpp", "gauge_test.c", "ack_test.c"):
            cpp = name.endswith(".cpp")
            compiler = shlex.split(os.environ["CXX" if cpp else "CC"])
            binary = Path(directory) / Path(name).stem
            subprocess.run(compiler + ["-std=c++17" if cpp else "-std=c11", "-g"] + flags +
                           [str(HERE / name), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
        for name in ("ask_test.py", "mailbox_test.py", "content_length_test.py", "identity_test.py", "approval_test.py"):
            subprocess.run([sys.executable, str(HERE / name)], check=True)


if __name__ == "__main__":
    main()
