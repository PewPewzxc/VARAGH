# Dictionaries in VARAGH

VARAGH ships no dictionary data: every dictionary has its own licence, so each
reader downloads the ones they want. The reader uses StarDict dictionaries
(`.ifo`, `.idx`, `.dict`); see [CrossInk's dictionary guide](../dictionary.md)
for all the details.

## German → English (recommended)

1. Download the **German – English** dictionary (`deu-eng`) in **StarDict**
   format from FreeDict: https://freedict.org/downloads/
2. Prepare it with Inky's dictionary tools, https://inky.crossink.dev/#dictionary-tools :
   open **Dictionary Tools**, add the `.ifo`, `.idx` and `.dict`/`.dict.dz` files,
   choose **Prepare Dictionary** and download the ZIP. Inky adds the
   `.idx.oft` and `.idx.oft.cspt` index files that make lookups fast.
3. Unzip it to `/dictionaries/German-English/` on the SD card
   (keep all files of the dictionary together in that folder).
4. Select NotoVazir as the reader font (Settings → Reader → Font Options →
   Font Family) so pronunciations and symbols display exactly.

In a German book, hold a word to look it up. VARAGH finds the dictionary form of
inflected words (Häuser → Haus, gemacht → machen) and lets you save the word
with its meaning to Flashcards with **Add to…**.

FreeDict's German-English dictionary is licensed GPL-3.0 / AGPL-3.0 (it comes
from the Ding dictionary of TU Chemnitz). You may use it freely; if you share
copies, share them under the same licence and point to the source.
