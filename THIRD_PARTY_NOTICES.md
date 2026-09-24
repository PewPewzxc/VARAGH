# Third-party notices

VARAGH's own source code is released under the MIT licence (see [LICENSE](LICENSE)).
This file lists the third-party code, fonts and data VARAGH builds on or ships,
and what each licence asks of anyone who redistributes them.

## Projects VARAGH is based on

| Project | Licence | Notes |
|---|---|---|
| [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) | MIT, © 2025 Dave Allie | Original firmware. Copyright notice kept in [LICENSE](LICENSE). |
| [CrossInk](https://github.com/uxjulia/crossink) | MIT (same LICENSE) | The fork VARAGH is based on. |
| [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) | MIT, © 2026 FreeInk | Display, input and UI library (git submodule `freeink-sdk`, VARAGH fork). Includes code derived from the [OpenX4 community SDK](https://github.com/open-x4-epaper/community-sdk) (MIT); see `freeink-sdk/LICENSE` and `freeink-sdk/NOTICE`. |
| CrossPoint pull requests #3616 and #3633 | MIT | Shared SD-font interval tables and faster glyph drawing, ported into VARAGH. |

## Libraries compiled into the firmware

Fetched by PlatformIO at the versions pinned in `platformio.ini`, or vendored in `lib/`.

| Library | Licence |
|---|---|
| wolfSSL (`wolfssl/Arduino-wolfSSL` 5.7.2) | GPL-2.0-or-later |
| WebSockets (`links2004/WebSockets` 2.7.3) | LGPL-2.1 |
| PNGdec, JPEGDEC (BitBank Software) | Apache-2.0 |
| ArduinoJson (Benoit Blanchon) | MIT |
| QRCode (Richard Moore) | MIT |
| SdFat (Bill Greiman) | MIT |
| expat, miniz, MiniBidi (`lib/`) | MIT |
| uzlib (`lib/`) | zlib licence |
| Tabler Icons (submodule `assets/tabler-icons`), Lucide icons (inside the SDK) | MIT, ISC |

**Firmware binaries.** Because `firmware-*.bin` links wolfSSL (GPL-2.0-or-later),
each released binary is distributed under the terms of the GNU GPL version 3 or
later as a whole. The complete corresponding source is this repository at the
release tag, together with the `freeink-sdk` submodule commit it points to; every
release also attaches a `varagh-*-full-source.zip` with both. The MIT licence
still applies to VARAGH's own source files.

## Fonts compiled into the firmware

All under the [SIL Open Font License 1.1](https://openfontlicense.org). The source
font files and their licence texts are in `lib/EpdFont/builtinFonts/source/`.

| Font | Copyright |
|---|---|
| Bitter | © 2011 The Bitter Project Authors |
| Lexend Deca | © 2019 The Lexend Project Authors |
| Inter (menu font and the VARAGH IPA supplement) | © 2016 The Inter Project Authors |
| Noto Sans, Noto Sans Arabic, Noto Sans Symbols 1/2 | © 2022 The Noto Project Authors |
| Noto Sans CJK SC | © 2014-2021 Adobe |
| IBM Plex Sans Hebrew (Hebrew letters in the menu font) | © 2017 IBM Corp., Reserved Font Name "Plex" (not used in any font name here) |
| ChareInk7 | Modified from SIL Charis, © 1997-2025 SIL Global; renamed because "Charis" and "SIL" are Reserved Font Names |

## Fonts in the release SD-card pack

`NotoVazir` (in `varagh-sd-card-*.zip`) is a converted, merged font made from
Noto Serif and Noto Sans Math (© 2022 The Noto Project Authors) and Vazirmatn
(© 2015 The Vazirmatn Project Authors). All three are under the SIL Open Font
License 1.1 with no Reserved Font Name, so the merged font may be distributed
under the same licence; the licence texts are included in the zip.
How it is built: [docs/varagh/FONTS.md](docs/varagh/FONTS.md).

## Not included: dictionaries

VARAGH ships no dictionary data. The German-English dictionary recommended in
[docs/varagh/DICTIONARIES.md](docs/varagh/DICTIONARIES.md) comes from
[FreeDict](https://freedict.org) (GPL-3.0 / AGPL-3.0) and is downloaded by each
user. Dictionaries built from Wiktionary are CC BY-SA 4.0 and must credit
Wiktionary and keep that licence if published.

## Name and logo

"VARAGH" and the "AP" logo (`src/images/BootLogo192.h`, `web/assets/logo.png`)
belong to PewPewzxc and are not covered by the MIT licence. If you publish your
own fork, please use your own name and logo.
