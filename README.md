<h1 align="center">VARAGH</h1>

<p align="center">
  <b>E-reader firmware for the Xteink X4 Pro, made for reading and learning in English, German and Persian.</b>
</p>

<p align="center">
  <a href="https://github.com/PewPewzxc/VARAGH/releases/latest"><img src="https://img.shields.io/github/v/release/PewPewzxc/VARAGH?label=release" alt="Latest release"></a>
  <a href="https://github.com/PewPewzxc/VARAGH/releases"><img src="https://img.shields.io/github/downloads/PewPewzxc/VARAGH/total?label=downloads" alt="Downloads"></a>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/PewPewzxc/VARAGH" alt="MIT licence"></a>
  <a href="https://github.com/PewPewzxc/VARAGH/stargazers"><img src="https://img.shields.io/github/stars/PewPewzxc/VARAGH?style=flat" alt="Stars"></a>
</p>

<p align="center">
  <a href="https://github.com/PewPewzxc/VARAGH/releases/latest"><b>Download</b></a> ·
  <a href="FEATURES.md"><b>Features</b></a> ·
  <a href="#install"><b>Install</b></a> ·
  <a href="CHANGELOG.md"><b>Changelog</b></a> ·
  <a href="https://github.com/PewPewzxc/VARAGH/discussions"><b>Discussions</b></a>
</p>

<p align="center">
  <img src="docs/images/varagh/cover.jpg" width="420" alt="VARAGH on the Xteink X4 Pro, with a Persian, a German and an English book on the Home screen">
</p>

**VARAGH** (ورق, Persian for "a sheet / a page") is e-reader firmware for the
**Xteink X4 Pro**, built for reading and learning in **English, German and Persian**.
It is a fork of [CrossInk](https://github.com/uxjulia/crossink), itself a fork of
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader); all of
their features are kept.

> VARAGH is an independent community project, not affiliated with or endorsed by
> Xteink, CrossInk or CrossPoint.

**Contents:** [New in 1.3.0](#new-in-130-lion-) · [Earlier releases](#earlier-releases) ·
[Features](#features) · [Install](#install) · [Build from source](#build-from-source) ·
[Contributing](#contributing) · [Support](#support) · [Credits](#credits-and-licences)

## New in 1.3.0 "Lion" 🦁

- **Control Center:** swipe down for the light and warmth sliders with exact
  1 % − / + buttons, and eight buttons you choose (Settings → Controls).
- **Night light:** a second brightness and warmth from evening to morning.
- **Dictionaries per language:** one page with a tab per language, each with
  its own card dictionary and two online sources. New sources: Datamuse,
  OpenThesaurus and English Wiktionary, asked by themselves when Wi-Fi is on.
- **Persian menus**, a Persian keyboard and **Caps Lock**.
- **Better word lookup** (went → go, می‌روم → رفتن) and **resizing a
  selection** with handles or the buttons.
- **Rotation that turns the buttons and touch** with the page.
- **One bold icon style** across the whole firmware, made for e-ink.
- **Go** and **Tic Tac Toe**, and **Next in series** at the end of a book.

<table>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.0/control-center.png" width="250" alt="The Control Center with light and warmth sliders and eight buttons"><br><b>Control Center</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.0/control-center-buttons.png" width="250" alt="Settings, Controls, Control Center: eight button slots"><br><b>Choose its buttons</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.0/dictionaries.png" width="250" alt="The Dictionaries page with a tab per language"><br><b>Dictionaries per language</b></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.0/online-sources.png" width="250" alt="Choosing the first online source: Wiktionary, Datamuse, Wikipedia"><br><b>Online sources</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.0/ask-resize.png" width="250" alt="Three words selected with handles and the Ask bubble"><br><b>Resize a selection</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.0/word-forms.png" width="250" alt="Looking up went opens go"><br><b>went → go</b></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.0/home-icons.png" width="250" alt="Home with the new bold icons and battery icon"><br><b>One icon style</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.0/apps.png" width="250" alt="Apps with Go and Tic Tac Toe"><br><b>Go and Tic Tac Toe</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.0/go.png" width="250" alt="A game of Go on the 9 by 9 board"><br><b>Go</b></td>
  </tr>
</table>

<sub>Screens captured from the 1.3.0 firmware running in the PC simulator.</sub>

See the [changelog](CHANGELOG.md) for everything.

## Earlier releases

<details>
<summary>What came in 1.2.1, 1.2.0 and 1.1.0 "Tiger" (click to open)</summary>

### New in 1.2.1 "Tiger"

- **Tap to Look Up:** a tap on a word opens its meaning, with no holding
  (Settings → Reader, off by default; swipe to turn pages while it is on).
- **Long Press on a Word:** choose what a long press does: look the word up,
  highlight it, add it to a category, or **Ask** with a bubble at the words.
  **Hold to Select** still sets only how long you hold; see
  [how the three settings work together](FEATURES.md#three-settings-three-questions-121).
- **Book ratings:** five stars with halves, at the end of a book, in the book
  menu, on Reading Stats and in the Library list. Kept as plain text on the card.
- **Apps stay open across sleep**, and a **stronger chess computer**.
- **Online lookups cannot hang** any more: Back gives one up at once.
- **A new logo** on the boot and sleep screens.

<table>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.1/tap-to-look-up.png" width="250" alt="The meaning of a tapped word shown over the book page"><br><b>Tap to Look Up</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.1/long-press-ask.png" width="250" alt="A selected word with the bubble Look Up, Highlight, Add to"><br><b>Long press: Ask</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.1/long-press-setting.png" width="250" alt="Settings, Reader: the four choices of Long Press on a Word"><br><b>Long Press on a Word</b></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.1/rating-screen.png" width="250" alt="The rating screen with three and a half stars"><br><b>Book rating</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.1/end-of-book-rating.png" width="250" alt="End of book with the title, the rating and what to continue with"><br><b>End of book</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.1/library-stars.png" width="250" alt="Library with a book's stars at the end of its row"><br><b>Stars in Library</b></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.1/boot-logo.png" width="250" alt="Boot screen with the open-book logo, VARAGH, Booting and 1.2.1 Tiger"><br><b>New logo</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.1/sleep-logo.png" width="250" alt="Dark sleep screen with the logo in light lines"><br><b>Sleep screen</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.1/book-menu-rating.png" width="250" alt="The book menu with Rating and its stars as the first row"><br><b>Rating in the book menu</b></td>
  </tr>
</table>

<sub>Screens captured from the 1.2.1 firmware running in the PC simulator.</sub>

### Since 1.2.0 "Tiger"

- **Apps on the Home screen:** Wordle, Sudoku, Chess, Checkers and a Clock with
  a calendar, made for the touch screen. Hide the ones you do not use.
- **Online dictionary:** look a word up on Wiktionary in English, German or
  Persian from inside a book; every answer is kept for offline use. With the
  optional **Keep Wi-Fi On** a new word takes a few seconds.
- **Faster Home:** the Carousel follows a swipe at once, and every theme paints
  Home once instead of two or three times.
- **Clock & Calendar sleep screen**, with the Persian date.

<table>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/apps-menu.png" width="250" alt="The Apps menu with tiles for Wordle, Sudoku, Chess, Checkers and Clock"><br><b>Apps</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/wordle.png" width="250" alt="Wordle with two guesses made and the keyboard showing what is known about each letter"><br><b>Wordle</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/sudoku.png" width="250" alt="A Medium Sudoku with a cell selected and the number keys and tools below"><br><b>Sudoku</b></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/chess.png" width="250" alt="Chess against the computer after two moves each"><br><b>Chess</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/checkers.png" width="250" alt="Checkers against the computer after the first moves"><br><b>Checkers</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/apps-manage.png" width="250" alt="Manage apps: a switch for each app and Settings for the Clock"><br><b>Manage</b></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/clock-large-digits.png" width="250" alt="Clock with large digits above the date, the Persian date and a month calendar"><br><b>Clock: large digits</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/clock-analog.png" width="250" alt="Clock with an analog face above the month calendar"><br><b>Clock: analog</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/clock-persian-days.png" width="250" alt="Clock with a calendar that shows the Persian day under each date"><br><b>Clock: Persian days</b></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/switch-dictionary-list.png" width="250" alt="Switch Dictionary list ending with Online (Wiktionary)"><br><b>Switch Dictionary → Online</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/online-dictionary.png" width="250" alt="A Wiktionary answer for the word fortune shown over the book page"><br><b>Online dictionary</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.2.0/sleep-clock-calendar.png" width="250" alt="Sleep screen with the weekday, a large day number, the Persian date and the month calendar"><br><b>Clock &amp; Calendar sleep screen</b></td>
  </tr>
</table>

<sub>Screens captured from the 1.2.0 firmware running in the PC simulator.</sub>

### Since 1.1.0 "Tiger"

- **Smoother, sharper pictures in books:** no more crosshatch pattern, clean white paper.
- **Chapters open faster:** long chapters show their first page right away
  (new Indexing Method **Automatic**), no "Indexing" popup for normal chapters,
  a faster SD card and fonts kept in memory.
- **Battery:** the processor slows down sooner, and buttons still respond instantly.

<p align="center">
  <img src="docs/images/varagh/tiger/shading-landscape.png" alt="The same picture on the X4 Pro screen: crosshatch pattern in 1.0.1, smooth greys in 1.1.0">
</p>

All changes: [CHANGELOG.md](CHANGELOG.md).

</details>

## Features

- **Persian and Arabic:** books switch to a font that can show them, and Persian
  words and pronunciations display correctly with any reader font (NotoVazir).
- **German → English dictionary:** finds the dictionary form of inflected words,
  with pronunciations.
- **Highlights and flashcards:** Favorite Lines, your own categories, **Add to…**
  from the dictionary, flashcards with *Again* / *Know it*.
- **Online dictionary:** Wiktionary and Wikipedia over Wi-Fi, saved on the card
  after the first lookup.
- **Apps:** Wordle, Sudoku, Chess, Checkers, Clock & Calendar.
- **Touch:** adjustable hold to select, selection handles, swipe left to delete.
- **Battery and speed:** the display powers down when idle, smoother pictures,
  faster chapters.

<table>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/persian-book.jpg" width="250" alt="A Persian book in NotoVazir"><br><b>Persian books</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/german-dictionary.jpg" width="250" alt="German to English dictionary entry with pronunciation"><br><b>German → English</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/flashcard-back.jpg" width="250" alt="Back of a flashcard with Again and Know it buttons"><br><b>Flashcards</b></td>
  </tr>
</table>

Every feature, with photos and how to use it: **[FEATURES.md](FEATURES.md)**.

## Install

1. Download `firmware-x4-pro.bin` from the latest
   [release](https://github.com/PewPewzxc/VARAGH/releases), copy it to the SD card and
   flash it from **Settings → System → SD Card Firmware Update**
   (coming from the original Xteink firmware, see the
   [installation guide](docs/installation.md) first).
2. From the same release, copy the `.fonts/NotoVazir` folder of
   `varagh-sd-card-*.zip` to `/.fonts/NotoVazir/` on the SD card.
3. Select **Settings → Reader → Font Options → Font Family → NotoVazir**.
4. Optional German → English dictionary: see [Dictionaries](docs/varagh/DICTIONARIES.md).

**Updating:** Settings → System → Check for Updates installs the newest VARAGH
release (from 1.0.0, update from the SD card once). Only install VARAGH firmware:
a CrossInk or CrossPoint release replaces VARAGH, and the reader warns you first.

## Build from source

Download `varagh-*-full-source.zip` from a release (it includes the display
library VARAGH uses), unpack it and run:

```bash
pio run -e x4-pro
```

The firmware is written to `.pio/build/x4-pro/firmware-x4-pro.bin`.
Developer notes: [docs/development](docs/development),
[keeping up with CrossInk](docs/varagh/UPSTREAM-SYNC.md).

## Contributing

Bug reports, ideas and pull requests are welcome. Please read
[CONTRIBUTING.md](CONTRIBUTING.md) first. Ideas and questions go to
[Discussions](https://github.com/PewPewzxc/VARAGH/discussions); bugs go to
[Issues](https://github.com/PewPewzxc/VARAGH/issues/new/choose).

## Support

VARAGH is free and made in spare time. If it makes your reading better, you can
say thanks with a coffee:

<a href="https://buymeacoffee.com/PewPewzxc"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" height="50" alt="Buy Me a Coffee"></a>

A ⭐ on GitHub helps other readers find the project too.

## Credits and licences

VARAGH is released under the [MIT licence](LICENSE), like CrossInk and CrossPoint.
Third-party code, fonts and data are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

- CrossPoint Reader: Dave Allie and contributors; CrossInk: uxjulia and contributors
- FreeInk SDK (display and UI library): FreeInk
- Fonts: Noto (Google / Noto Project Authors), Vazirmatn (Saber Rastikerdar and the
  Vazirmatn Project Authors), Inter (Rasmus Andersson and the Inter Project Authors)
- Speed ideas in 1.1.0 were inspired by studying
  [microreader](https://github.com/CidVonHighwind/microreader) (no code copied).
- VARAGH was co-authored by Claude (Anthropic).

Dictionary data is not included; see [Dictionaries](docs/varagh/DICTIONARIES.md).
