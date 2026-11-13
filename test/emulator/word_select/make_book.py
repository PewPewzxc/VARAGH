"""Builds the word-select test book: one short chapter per page type that broke or could break
the word-select working set (line-end book hyphens, Persian, a table, a footnote link)."""
import sys
import zipfile

CHAPTERS = [
    ("Hyphens", "<p>" + " ".join(
        ["context, be it an internet-capable smartwatch, smartphone, gaming console, laptop, desktop computer, "
         "or TV (Figure 5-2). Today, responsive web design is the de facto standard, a well-known self-aware "
         "state-of-the-art long-term high-quality user-friendly real-time cross-platform"] * 3) + "</p>"),
    ("Persian", '<p dir="rtl">' + " ".join(
        ["به نام خداوند جان و خرد کزین برتر اندیشه برنگذرد خداوند نام و خداوند جای خداوند روزی‌ده رهنمای "
         "می‌رود نیم‌فاصله کتاب‌ها دانش‌آموز"] * 4) + "</p>"),
    ("Table", "<p>Before the table there is a self-aware line of text.</p><table>"
              "<tr><th>Device</th><th>Screen-size</th><th>Input</th></tr>"
              "<tr><td>smartwatch</td><td>very-small</td><td>touch-only</td></tr>"
              "<tr><td>laptop</td><td>medium-large</td><td>keyboard</td></tr></table>"
              "<p>After the table the well-known text goes on for a line or two more.</p>"),
    ("Footnote", '<p>A sentence with a note<a href="#n1" id="r1"><sup>1</sup></a> in the middle and a long-term '
                 'plan that keeps going so the page has several lines of ordinary words to press on.</p>'
                 '<p id="n1">1. The note itself, with a cross-reference.</p>'),
]


def main(out):
    items = "".join(f'<item id="c{i}" href="c{i}.xhtml" media-type="application/xhtml+xml"/>' for i in range(len(CHAPTERS)))
    refs = "".join(f'<itemref idref="c{i}"/>' for i in range(len(CHAPTERS)))
    nav = "".join(f'<navPoint id="n{i}" playOrder="{i + 1}"><navLabel><text>{t}</text></navLabel>'
                  f'<content src="c{i}.xhtml"/></navPoint>' for i, (t, _) in enumerate(CHAPTERS))
    with zipfile.ZipFile(out, "w") as z:
        z.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        z.writestr("META-INF/container.xml",
                   '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">'
                   '<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>'
                   '</rootfiles></container>')
        z.writestr("OEBPS/content.opf",
                   '<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id">'
                   '<metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Word Select Test</dc:title>'
                   '<dc:language>en</dc:language><dc:identifier id="id">varagh-word-select-test</dc:identifier></metadata>'
                   f'<manifest>{items}<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/></manifest>'
                   f'<spine toc="ncx">{refs}</spine></package>')
        z.writestr("OEBPS/toc.ncx",
                   '<?xml version="1.0"?><ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head/>'
                   f'<docTitle><text>Word Select Test</text></docTitle><navMap>{nav}</navMap></ncx>')
        for i, (title, body) in enumerate(CHAPTERS):
            z.writestr(f"OEBPS/c{i}.xhtml",
                       '<?xml version="1.0" encoding="utf-8"?><html xmlns="http://www.w3.org/1999/xhtml"><head>'
                       f'<title>{title}</title></head><body><h1>{title}</h1>{body}</body></html>')


if __name__ == "__main__":
    main(sys.argv[1])
