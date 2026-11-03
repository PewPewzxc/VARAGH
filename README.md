<h1 align="center">VARAGH</h1>

<p align="center">
  <b>E-reader firmware for the Xteink X4 Pro, X4 Classic and X3, made for reading and learning in English, German and Persian.</b>
</p>

<p align="center">
  <a href="https://github.com/PewPewzxc/VARAGH/releases/latest"><img src="https://img.shields.io/github/v/release/PewPewzxc/VARAGH?label=release" alt="Latest release"></a>
  <a href="https://github.com/PewPewzxc/VARAGH/releases"><img src="https://img.shields.io/github/downloads/PewPewzxc/VARAGH/total?label=downloads" alt="Downloads"></a>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/PewPewzxc/VARAGH" alt="MIT licence"></a>
  <a href="https://github.com/PewPewzxc/VARAGH/stargazers"><img src="https://img.shields.io/github/stars/PewPewzxc/VARAGH?style=flat" alt="Stars"></a>
</p>

<p align="center">
  <a href="https://github.com/PewPewzxc/VARAGH/releases/latest"><b>Download</b></a> ·
  <a href="https://pewpewzxc.github.io/reader-simulator/"><b>Try it in your browser</b></a> ·
  <a href="FEATURES.md"><b>Features</b></a> ·
  <a href="#install"><b>Install</b></a> ·
  <a href="CHANGELOG.md"><b>Changelog</b></a> ·
  <a href="https://github.com/PewPewzxc/VARAGH/discussions"><b>Discussions</b></a>
</p>

<p align="center">
  <img src="docs/images/varagh/cover.jpg" width="420" alt="VARAGH 1.3.1 asleep on the Xteink X4 Pro: the Shahnameh, a German and an English book on Home, under a drawing used as the sleep picture">
</p>

**VARAGH** (ورق, Persian for "a sheet / a page") is e-reader firmware for the
**Xteink X4 Pro**, **X4 Classic** and **X3**, built for reading and learning in
**English, German and Persian**. The X4 Pro gets everything; the X4 Classic and X3
have no touch screen or front light, so they work everything with the buttons
(see [Readers without touch](#readers-without-touch)).
It is a fork of [CrossInk](https://github.com/uxjulia/crossink), itself a fork of
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader); all of
their features are kept.

> VARAGH is an independent community project, not affiliated with or endorsed by
> Xteink, CrossInk or CrossPoint.

**Contents:** [Why VARAGH](#why-varagh) · [Try it first](#try-it-first) ·
[New in 1.3.1](#new-in-131-lion-) · [Version history](#version-history) ·
[Install](#install) · [Build from source](#build-from-source) ·
[Contributing](#contributing) · [Support](#support) · [Credits](#credits-and-licences)

## Why VARAGH

VARAGH turns the Xteink readers into a tool for reading and learning in English,
German and Persian. On top of everything CrossInk and CrossPoint already do, it adds
the following, release by release since 1.0.0.

### Languages

- **English, German and Persian built in:** every reader shows all three straight
  after installing, with no font to copy to the SD card.
- **Persian and Arabic script:** right-to-left lines, joined letters, vowel marks,
  Persian digits and «» quotes; mixed English and Persian paragraphs keep their
  direction.
- **Pronunciation letters:** IPA such as /θɪŋk/ or [haʊ̯s] in books, menus,
  dictionaries and flashcards.
- **Font fallback:** books that need Arabic script switch to a font that covers it;
  downloaded fonts borrow the Persian letters they lack.
- **Menus in 29 languages:** English and Deutsch built in; Persian and 26 more as
  small downloads.
- **Keyboards:** a Persian layout and Caps Lock.

### Dictionaries and word lookup

- **A dictionary per language:** one page with a tab for each language, its own
  offline dictionary and two online sources.
- **Online sources:** Wiktionary, Wikipedia, Datamuse (WordNet) and OpenThesaurus,
  saved on the card after the first lookup; optional Keep Wi-Fi On.
- **Word forms:** went → go, träumte → träumen, می‌روم → رفتن, with German
  stemming and repair of broken apostrophes and hyphens.
- **Remembers your choice:** the last dictionary stays the default; Switch
  Dictionary at any time.
- **Lookups never hang:** Back cancels an online lookup at once.

### Selecting words

- **Tap to Look Up:** one tap on a word opens its meaning.
- **Long Press on a Word:** look up, highlight, add to a category, or Ask with a
  bubble at the words.
- **Hold to Select:** Fast, Normal, Slow or Very slow.
- **Selection handles** to resize a selection, on the touch screen or with the
  buttons.

### Highlights and flashcards

- **Highlights hub on Home:** Favorite Lines, your own categories and By Book.
- **Add to…** straight from the dictionary.
- **Flashcards:** Leitner boxes, *Again* / *Know it* and Practice, with the
  pronunciation on the back.
- **Swipe left to delete** in files, recent books, highlights, bookmarks, lookup
  history and categories.

### Books

- **Drop caps, text and heading sizes**, bordered and lightly shaded boxes, and
  tables, as the publisher made them.
- **Smooth grey pictures** without a crosshatch pattern, on clean white paper.
- **Line breaks after hyphens and dashes.**
- **Book ratings:** five stars with halves, at the end of a book, in the book menu,
  in Reading Stats and in Library.
- **Next in series** at the end of a book.

### Screen, light and controls

- **Settings in eight groups** with an icon and a short description each.
- **Control Center:** swipe down for light and warmth with exact 1 % steps and
  eight buttons you choose (X4 Pro).
- **Night light:** a second brightness and warmth from evening to morning (X4 Pro).
- **Rotation** that turns the buttons and the touch screen with the page.
- **Bluetooth page turner:** pair a remote and map its buttons.
- **One bold icon style** made for e-ink, and the VARAGH logo on the boot and
  sleep screens.

### Apps and clock

- **Games:** Wordle, Sudoku with five levels, a daily puzzle and explaining hints, Chess, Checkers, Go and
  Tic Tac Toe; apps stay open across sleep; hide the ones you do not use.
- **Clock & Calendar** with the Persian date, also as a sleep screen.
- **A clock that stays right:** set on every Wi-Fi connect, Automatic Time Zone,
  Summer Time and an optional Live Clock.

### Speed and battery

- **Fast chapters:** long chapters show their first page at once (Automatic
  indexing), with a faster SD card and fonts kept in memory.
- **Fast picture pages:** the next page's pictures are prepared while you read.
- **Battery:** the processor slows down sooner and the display powers down when
  idle, while buttons still respond instantly.

### Readers and updates

- **Three readers:** the X4 Pro with touch and front light; the X4 Classic and X3
  with every feature on the buttons.
- **Safe updates:** Check for Updates installs the newest VARAGH release, and the
  reader warns before a firmware from another project replaces it.

<table>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/mixed-languages.png" width="250" alt="One page with English, a Persian line, German words, a table and pronunciations"><br><b>Three languages on one page</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/persian-book.png" width="250" alt="The first page of the Shahnameh in Persian"><br><b>Persian books</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/german-lookup-ipa.png" width="250" alt="German dictionary entry for Haus with its pronunciation over the book page"><br><b>German → English</b></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/persian-lookup.png" width="250" alt="The Persian word خرد looked up in a Persian dictionary"><br><b>Persian → Persian</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/flashcards.png" width="250" alt="A flashcard deck with German words and the Practice button"><br><b>Flashcards</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/home.png" width="250" alt="Home with the Shahnameh cover and the menu"><br><b>Home</b></td>
  </tr>
</table>

Every feature, with photos and how to use it: **[FEATURES.md](FEATURES.md)**.

### Try it first

The **[Reader Simulator](https://pewpewzxc.github.io/reader-simulator/)** runs
VARAGH in your web browser, on an X4 Pro, X4 Classic or X3 screen, with no reader
needed. Open your own books in it, try the dictionary and the apps, then decide.

## New in 1.3.1 "Lion" 🦁

- **Persian, German and pronunciations built in** on every reader: no font to
  copy to the SD card any more, and downloaded fonts such as Bitter borrow the
  Persian letters they lack.
- **Settings is a list of eight groups** with an icon and a short line each.
- **Books look like books:** drop caps, CSS text and heading sizes, bordered and
  lightly shaded boxes, and line breaks after dashes.
- **Bluetooth page turner:** pair a remote and map its buttons.
- **Faster picture pages:** the next page's pictures are prepared while you read.
- **Sudoku:** an answer check, Expert and Master levels, a daily puzzle,
  explaining hints and nine switches in its settings.
- A **clock that stays right**: it is set on every
  Wi-Fi connect, with an Automatic Time Zone and an optional Live Clock.

<table>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/settings-groups.png" width="250" alt="Settings as eight groups with icons and descriptions"><br><b>Settings in groups</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/drop-cap.png" width="250" alt="A chapter opening with a large drop cap over three lines"><br><b>Drop caps</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/box.png" width="250" alt="A tip in a bordered box with a light grey background"><br><b>Boxes</b></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/table.png" width="250" alt="A table with English, German and Persian words"><br><b>Tables</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/picture.png" width="250" alt="A lake at dawn drawn in smooth greys"><br><b>Pictures</b></td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/flashcard-back-ipa.png" width="250" alt="The back of a flashcard for Häuser with its pronunciation"><br><b>Pronunciations everywhere</b></td>
  </tr>
</table>

<sub>Screens captured from the 1.3.1 firmware running in the Reader Simulator.</sub>

See the [changelog](CHANGELOG.md) for everything.

## Version history

| Version | Date | What it added |
|---|---|---|
| **1.3.1 "Lion"** | 2026-11-03 | Persian, German and pronunciations built in on every reader, Settings in groups, drop caps, boxes and text sizes, Bluetooth page turner, faster picture pages, Sudoku answer check, levels, daily puzzle and explaining hints, clock sync and Live Clock |
| **1.3.0 "Lion"** | 2026-10-20 | Control Center and night light, dictionaries per language with online sources, Persian menus and keyboard, went → go lookups, resizing a selection, rotation, one icon style, Go and Tic Tac Toe; the X4 Classic and X3 join with button apps and menu-language downloads |
| **1.2.1 "Tiger"** | 2026-10-13 | Tap to Look Up, Long Press choices with Ask, book ratings, apps that stay open across sleep, a stronger chess computer, the new logo |
| **1.2.0 "Tiger"** | 2026-10-07 | Apps on Home (Wordle, Sudoku, Chess, Checkers, Clock & Calendar), online Wiktionary dictionary, faster Home, Clock & Calendar sleep screen |
| **1.1.0 "Tiger"** | 2026-10-01 | Smooth grey pictures, faster chapters (Automatic indexing), a faster SD card, longer battery life |
| **1.0.1** | 2026-09-24 | Check for Updates and KOReader Sync no longer restart the X4 Pro |
| **1.0.0** | 2026-09-24 | First release: Persian books, German → English lookup with word forms, Highlights and flashcards, swipe to delete, Hold to Select with handles, Summer Time |

Every change, version by version: **[CHANGELOG.md](CHANGELOG.md)**.

### Earlier releases

<details>
<summary>What came in 1.3.0 "Lion", 1.2.1, 1.2.0 and 1.1.0 "Tiger", with pictures (click to open)</summary>

### New in 1.3.0 "Lion" 🦁

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

</details>

## Install

1. Download the file for your reader from the latest
   [release](https://github.com/PewPewzxc/VARAGH/releases/latest): `firmware-x4-pro.bin`,
   `firmware-x4-classic.bin`, or `firmware-x3-x4.bin` (X3 and the original X4).
   Copy it to the SD card and
   flash it from **Settings → System → SD Card Firmware Update**
   (coming from the original Xteink firmware, see the
   [installation guide](docs/installation.md) first).
2. That is all: English, German and Persian books and pronunciations work with
   the built-in fonts.
3. Optional: a German → English dictionary, see [Dictionaries](docs/varagh/DICTIONARIES.md).
4. Optional: NotoVazir, a different look for Persian. Copy the `.fonts/NotoVazir`
   folder of `varagh-sd-card-*.zip` from the release to `/.fonts/NotoVazir/` on the
   SD card and select **Settings → Reader → Font Options → Font Family → NotoVazir**.

### Readers without touch

<table>
  <tr>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/control-center.png" width="230" alt="X4 Pro: the Control Center"><br><b>X4 Pro</b>: Control Center</td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/x4-classic-apps.png" width="230" alt="X4 Classic: Apps with a frame on Tic Tac Toe"><br><b>X4 Classic</b>: apps with buttons</td>
    <td align="center" width="33%"><img src="docs/images/varagh/v1.3.1/x3-button-selection.png" width="230" alt="X3: three words selected with the buttons and Look Up, Highlight, Add to"><br><b>X3</b>: Look Up / Highlight / Add to</td>
  </tr>
</table>

On the X4 Classic and X3 the apps are played with a frame that the direction
buttons move and Confirm taps. In Look Up Word, hold Confirm, add words with
Left/Right and press Confirm for Look Up, Highlight or Add to. Not available there:
Control Center, front light, night light, Tap to Look Up and selection handles.
The X3/X4 firmware has English and German menus built in; other menu languages
are picked in Settings → Language and download over Wi-Fi, or copy the
`languages` folder of `varagh-v1.3.1-x3-x4-languages.zip` to the SD card.

**Updating:** Settings → System → Check for Updates installs the newest VARAGH
release (from 1.0.0, update from the SD card once). Only install VARAGH firmware:
a CrossInk or CrossPoint release replaces VARAGH, and the reader warns you first.

## Build from source

Download `varagh-*-full-source.zip` from a release (it includes the display
library VARAGH uses), unpack it and run:

```bash
pio run -e x4-pro       # X4 Pro
pio run -e x4-classic   # X4 Classic
pio run -e default      # X3 and the original X4
```

The firmware is written to `.pio/build/<env>/` (`firmware-x4-pro.bin`,
`firmware-x4-classic.bin`, `firmware-x3-x4.bin`). The `default` build also writes
the menu-language packs to `.pio/lang-packs/default/`.
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
