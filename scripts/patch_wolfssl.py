from pathlib import Path
import hashlib

Import("env")


PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
MARKER = "/* CrossPoint wolfSSL compatibility overrides */"
OVERRIDES = f"""

{MARKER}
#undef NO_DH
#ifndef HAVE_FFDHE_2048
#define HAVE_FFDHE_2048
#endif
#undef FP_MAX_BITS
#define FP_MAX_BITS 8192
/* AirDrop reuses the firmware TLS library instead of linking a second stack. */
#if CROSSPOINT_AIRDROP
#undef NO_WOLFSSL_SERVER
#define HAVE_SERVER_RENEGOTIATION_INFO
#define CROSSPOINT_TLS12_SERVER_ONLY
#endif
"""


def patch_user_settings(path: Path) -> None:
    original_text = path.read_text()
    text = original_text
    if MARKER in text:
        text = text.split(MARKER, 1)[0].rstrip()
    patched_text = text + OVERRIDES + "\n"
    if original_text == patched_text:
        return
    path.write_text(patched_text)
    print(f"Patched wolfSSL settings: {path.relative_to(PROJECT_DIR)}")


TLS13_MARKER = "/* CrossPoint: TLS 1.2 server, TLS 1.3 client */"
TLS13_SHA256 = "9e780a7defaca54422dd02cdfc4029bcc3062268c7430c48f04cb4376034e4ab"


def patch_tls13(path: Path) -> None:
    text = path.read_text()
    if TLS13_MARKER in text:
        return
    if hashlib.sha256(text.encode()).hexdigest() != TLS13_SHA256:
        raise RuntimeError(f"Unrecognized wolfSSL tls13.c; review server-only patch: {path}")
    # Change conditional compilation only, after headers have defined shared structs.
    # Never define NO_WOLFSSL_SERVER locally: that can change the library ABI.
    text = text.replace("#ifndef NO_WOLFSSL_SERVER", "#if !defined(NO_WOLFSSL_SERVER)")
    text = text.replace("#ifdef NO_WOLFSSL_SERVER", "#if defined(NO_WOLFSSL_SERVER)")
    text = text.replace("defined(NO_WOLFSSL_SERVER)",
                        "(defined(NO_WOLFSSL_SERVER) || defined(CROSSPOINT_TLS12_SERVER_ONLY))")
    # ssl.c dispatches here for a TLS 1.3 server. Reject it without linking the server.
    text += """
/* CrossPoint: TLS 1.2 server, TLS 1.3 client */
#if defined(WOLFSSL_TLS13) && defined(CROSSPOINT_TLS12_SERVER_ONLY) && !defined(NO_WOLFSSL_SERVER)
int wolfSSL_accept_TLSv13(WOLFSSL* ssl)
{
    if (ssl != NULL) ssl->error = VERSION_ERROR;
    return WOLFSSL_FATAL_ERROR;
}
#endif
"""
    path.write_text(text)


for settings in PROJECT_DIR.glob(".pio/libdeps/*/Arduino-wolfSSL/src/user_settings.h"):
    patch_user_settings(settings)
    patch_tls13(settings.parent / "src/tls13.c")
