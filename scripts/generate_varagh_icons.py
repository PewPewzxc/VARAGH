"""VARAGH icon system (1.3.0 Lion, style B "bold line"): 1-bit icons on a 24 px grid.

Every icon is drawn as vectors on the 24-unit grid at 8x, then thresholded to
1-bit at 24 px, so the same source makes the design sheet and later the
firmware bitmaps. Lines are drawn 1.4x the nominal width (2 -> 2.8 px) so they
stay dark on e-ink; a 32 px icon is the same drawing larger with the same line.

Running the script rewrites every icon header in src/components/icons with
the same symbol names and sizes, so no screen code changes.
"""
import argparse, math, re, sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

SS = 8          # supersample
G = 24          # grid
SW = 2          # nominal stroke width
WEIGHT = 1.4    # style B: every line 1.4x as heavy
FONT = str(Path(__file__).resolve().parent.parent / "lib/EpdFont/builtinFonts/source/Inter/Inter-")


class Pen:
    def __init__(self, size=G):
        self.size = size
        self.k = SS * size / G
        self.img = Image.new("L", (size * SS, size * SS), 0)
        self.d = ImageDraw.Draw(self.img)

    def p(self, x, y):
        return (x * self.k, y * self.k)

    def w(self, sw=SW):
        return round(sw * WEIGHT * SS)

    def line(self, *pts, sw=SW, ink=255):
        pts = [self.p(*q) for q in pts]
        self.d.line(pts, fill=ink, width=self.w(sw), joint="curve")
        r = self.w(sw) / 2
        for (x, y) in (pts[0], pts[-1]):
            self.d.ellipse([x - r, y - r, x + r, y + r], fill=ink)

    def circle(self, cx, cy, r, sw=SW, fill=False, ink=255):
        b = [*self.p(cx - r, cy - r), *self.p(cx + r, cy + r)]
        if fill:
            o = self.w(sw) / 2
            self.d.ellipse([b[0] - o, b[1] - o, b[2] + o, b[3] + o], fill=ink)
        else:
            o = self.w(sw) / 2
            self.d.ellipse([b[0] - o, b[1] - o, b[2] + o, b[3] + o], outline=ink, width=self.w(sw))

    def arc(self, cx, cy, r, a0, a1, sw=SW, ink=255, caps=True):
        o = self.w(sw) / 2
        b = [*self.p(cx - r, cy - r), *self.p(cx + r, cy + r)]
        self.d.arc([b[0] - o, b[1] - o, b[2] + o, b[3] + o], a0, a1, fill=ink, width=self.w(sw))
        if caps:
            for a in (a0, a1):
                x, y = self.p(cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a)))
                self.d.ellipse([x - o, y - o, x + o, y + o], fill=ink)

    def rrect(self, x0, y0, x1, y1, r, sw=SW, fill=False, ink=255):
        o = self.w(sw) / 2
        b = [self.p(x0, y0)[0] - o, self.p(x0, y0)[1] - o, self.p(x1, y1)[0] + o, self.p(x1, y1)[1] + o]
        rad = r * self.k + o
        if fill:
            self.d.rounded_rectangle(b, rad, fill=ink)
        else:
            self.d.rounded_rectangle(b, rad, outline=ink, width=self.w(sw))

    def poly(self, *pts, ink=255):
        self.d.polygon([self.p(*q) for q in pts], fill=ink)

    def pie(self, cx, cy, r, a0, a1, ink=255):
        self.d.pieslice([*self.p(cx - r, cy - r), *self.p(cx + r, cy + r)], a0, a1, fill=ink)

    def arrow_end(self, cx, cy, r, a, size=3.2, ink=255):
        """Solid arrowhead at angle a of a clockwise arc, pointing along the arc."""
        t = math.radians(a)
        x, y = cx + r * math.cos(t), cy + r * math.sin(t)
        dx, dy = -math.sin(t), math.cos(t)
        nx, ny = math.cos(t), math.sin(t)
        self.poly((x + dx * size * 1.2, y + dy * size * 1.2), (x + nx * size, y + ny * size),
                  (x - nx * size, y - ny * size), ink=ink)

    def bitmap(self):
        small = self.img.resize((self.size, self.size), Image.BOX)
        return small.point(lambda v: 0 if v >= 110 else 255).convert("1")


# ---------------------------------------------------------------- icons
# Each takes (pen, filled). Outline = "off"/normal; filled = "on"/selected.
# Details on a filled shape are knocked out in white (ink=0), with >= 2 px gaps.
def wifi(p, f):
    if f:
        p.pie(12, 20, 15, 225, 315)
        for r in (5.5, 10.5):
            p.arc(12, 20, r, 220, 320, ink=0, caps=False)
        return
    for r in (5.5, 10, 14.5):
        p.arc(12, 20, r, 228, 312)
    p.circle(12, 20, 1.4, fill=True)


def wifi_off(p, f):
    wifi(p, False)
    p.line((4, 4), (20, 20), sw=4, ink=0)
    p.line((4, 4), (20, 20))


def contrast(p, f):
    p.circle(12, 12, 8)
    p.pie(12, 12, 8, 90, 270)
    if f:
        p.circle(12, 12, 8, fill=True)
        p.pie(12, 12, 5.6, 270, 90, ink=0)


def refresh(p, f):
    p.arc(12, 12.5, 7, 20, 290)
    p.arrow_end(12, 12.5, 7, 292)


def lock(p, f):
    p.rrect(5, 11, 19, 20, 2, fill=f)
    p.arc(12, 10, 4, 180, 360, caps=False)
    p.line((8, 10), (8, 11))
    p.line((16, 10), (16, 11))
    if f:
        p.line((12, 14), (12, 16), ink=0)
    else:
        p.line((12, 14), (12, 16))


def lock_open(p, f):
    p.rrect(5, 11, 19, 20, 2, fill=f)
    p.arc(16, 7, 4, 180, 360, caps=False)
    p.line((12, 7), (12, 11))
    p.line((12, 14), (12, 16), ink=0 if f else 255)


def sync(p, f):
    p.arc(12, 12, 7, 200, 320)
    p.arrow_end(12, 12, 7, 322)
    p.arc(12, 12, 7, 20, 140)
    p.arrow_end(12, 12, 7, 142)


def power(p, f):
    if f:
        p.circle(12, 12, 9, fill=True)
        p.arc(12, 13, 5, 300, 240, ink=0)
        p.line((12, 6), (12, 12), ink=0)
    else:
        p.arc(12, 13, 7, 300, 240)
        p.line((12, 4), (12, 12))


def gear(p, f):
    for i in range(8):
        a = math.radians(i * 45 + 22.5)
        p.line((12 + 6.5 * math.cos(a), 12 + 6.5 * math.sin(a)), (12 + 8.5 * math.cos(a), 12 + 8.5 * math.sin(a)), sw=3.2)
    p.circle(12, 12, 6, fill=f)
    p.circle(12, 12, 6)
    p.circle(12, 12, 2.2, ink=0 if f else 255)


def reader_bar(p, f):
    p.rrect(4, 4, 20, 20, 3)
    if f:
        p.rrect(4, 4, 20, 8, 2, fill=True)
    else:
        p.line((4, 8), (20, 8))


def snowflake(p, f):
    for a in (90, 30, 150):
        t = math.radians(a)
        p.line((12 - 9 * math.cos(t), 12 - 9 * math.sin(t)), (12 + 9 * math.cos(t), 12 + 9 * math.sin(t)))
    for a in range(30, 390, 60):
        t = math.radians(a)
        x, y = 12 + 6 * math.cos(t), 12 + 6 * math.sin(t)
        for side in (-1, 1):
            u = t + side * math.radians(40)
            p.line((x, y), (x + 3 * math.cos(u), y + 3 * math.sin(u)), sw=1.8)


def smooth(pts, rounds=2, keep=()):
    """Chaikin corner cutting on a closed outline; points listed in keep stay sharp (flame tips)."""
    for _ in range(rounds):
        out = []
        n = len(pts)
        for i in range(n):
            p0, p1 = pts[i], pts[(i + 1) % n]
            if p0 in keep:
                out.append(p0)
            out.append((0.75 * p0[0] + 0.25 * p1[0], 0.75 * p0[1] + 0.25 * p1[1]))
            out.append((0.25 * p0[0] + 0.75 * p1[0], 0.25 * p0[1] + 0.75 * p1[1]))
        pts = out
    return pts


def flame(p, f):
    # Main tongue leaning right, a small tongue each side, an inner flame.
    tips = ((12.5, 2), (17.5, 7.5), (7.5, 6.5))
    outer = smooth([(12, 21.5), (16.5, 20.5), (19.5, 16.5), (19.5, 11.5), tips[1], (15.5, 10), (15.5, 6),
                    tips[0], (10.5, 7), (9.5, 10.5), tips[2], (5, 11.5), (4.5, 16), (7, 20.5)], keep=tips)
    inner_tip = (12, 11)
    inner = smooth([(12, 19.5), (14.5, 18.5), (15, 16), (13.5, 13.5), inner_tip, (10.5, 14), (9, 16.5),
                    (9.5, 18.5)], keep=(inner_tip,))
    if f:
        p.poly(*outer)
        p.line(*outer, outer[0])
        p.poly(*inner, ink=0)
    else:
        p.line(*outer, outer[0])
        p.line(*inner, inner[0], sw=1.8)


def sunset(p, f):
    # Evening sun on the horizon: the warm end of the light.
    p.arc(12, 16, 4.5, 180, 360, caps=False)
    if f:
        p.pie(12, 16, 4.5, 180, 360)
    for a in (200, 270, 340):
        t = math.radians(a)
        p.line((12 + 8 * math.cos(t), 16 + 8 * math.sin(t)), (12 + 10 * math.cos(t), 16 + 10 * math.sin(t)))
    p.line((3, 16), (21, 16))
    p.line((8, 20), (16, 20))


def candle(p, f):
    p.rrect(8, 12, 16, 21, 2, fill=f)
    p.rrect(8, 12, 16, 21, 2)
    p.poly((12, 2.5), (14.8, 7.2), (12, 9.8), (9.2, 7.2))
    p.d.ellipse([*p.p(9.4, 5), *p.p(14.6, 10.2)], fill=255)


def thermo_hot(p, f):
    warmth(p, True)
    for y in (5, 9):
        p.line((17, y), (20, y))


def moon(p, f):
    p.circle(12, 12, 8, fill=True)
    p.circle(15.5, 8.5, 7, fill=True, sw=0, ink=0)  # a solid crescent reads best at 24 px, either state


def minus(p, f):
    p.line((6, 12), (18, 12))


def plus(p, f):
    p.line((6, 12), (18, 12))
    p.line((12, 6), (12, 18))


def sun(p, f):
    p.circle(12, 12, 4, fill=f)
    p.circle(12, 12, 4)
    for i in range(8):
        a = math.radians(i * 45)
        p.line((12 + 7.5 * math.cos(a), 12 + 7.5 * math.sin(a)), (12 + 9 * math.cos(a), 12 + 9 * math.sin(a)))


def warmth(p, f):  # thermometer
    p.rrect(10, 3, 14, 14, 2)
    p.circle(12, 17, 4, fill=f)
    p.circle(12, 17, 4)
    p.line((12, 9), (12, 17) if f else (12, 15))
    if f:
        p.line((12, 9), (12, 15), ink=0)


def bulb(p, f):
    p.arc(12, 10, 6, 140, 40 + 360)
    p.line((8, 15), (9, 17))
    p.line((16, 15), (15, 17))
    p.line((9, 17), (15, 17))
    p.line((10, 20), (14, 20))
    if f:
        p.circle(12, 10, 6, fill=True)
        p.poly((7.5, 13), (16.5, 13), (15, 17), (9, 17))


def home(p, f):
    p.line((3, 11), (12, 3.5), (21, 11))
    p.rrect(6, 10, 18, 20, 1.5, fill=f)
    p.rrect(6, 10, 18, 20, 1.5)
    if f:
        p.rrect(10, 14, 14, 21, 0.5, fill=True, ink=0)
    else:
        p.line((10, 20), (10, 15), (14, 15), (14, 20))


def back(p, f):
    p.line((15, 4), (8, 12), (15, 20))


def close(p, f):
    p.line((6, 6), (18, 18))
    p.line((18, 6), (6, 18))


def search(p, f):
    p.circle(10.5, 10.5, 6, fill=f)
    p.circle(10.5, 10.5, 6)
    p.line((15, 15), (20, 20), sw=2.6)


def book(p, f):
    p.rrect(5, 3, 19, 21, 2, fill=f)
    p.rrect(5, 3, 19, 21, 2)
    p.line((5, 17), (19, 17), ink=0 if f else 255)


def library(p, f):
    p.rrect(3, 5, 7, 20, 1, fill=f)
    p.rrect(10, 5, 14, 20, 1, fill=f)
    p.d.polygon([p.p(16.5, 6.5), p.p(20, 5.5), p.p(23, 18.5), p.p(19.5, 19.5)], outline=255, width=p.w())
    if f:
        p.poly((16.5, 6.5), (20, 5.5), (23, 18.5), (19.5, 19.5))


def bookmark(p, f):
    if f:
        p.poly((6, 3), (18, 3), (18, 21), (12, 16), (6, 21))
    p.line((6, 3), (18, 3), (18, 21), (12, 16), (6, 21), (6, 3))


def highlight(p, f):
    p.d.polygon([p.p(14, 3), p.p(20, 9), p.p(11, 18), p.p(5, 12)], outline=255, width=p.w(), fill=255 if f else None)
    p.line((14, 3), (20, 9), (11, 18), (5, 12), (14, 3))
    p.line((5, 12), (3, 18), (8, 18))
    p.line((3, 21), (21, 21))


def dictionary(p, f):
    book(p, f)
    ink = 0 if f else 255
    p.line((9, 14), (12, 6.5), (15, 14), sw=1.8, ink=ink)
    p.line((10.2, 11.5), (13.8, 11.5), sw=1.6, ink=ink)


def text_size(p, f):
    p.line((3, 20), (8, 6), (13, 20))
    p.line((5, 15), (11, 15))
    p.line((14, 20), (17.5, 11), (21, 20))


def toc(p, f):
    for y in (6, 12, 18):
        p.circle(4.5, y, 1, fill=True)
        p.line((9, y), (20, y))


def rotate(p, f):
    p.rrect(4, 9, 14, 20, 2, fill=f)
    p.rrect(4, 9, 14, 20, 2)
    p.arc(14, 9, 6, 270, 360 + 30 - 360 if False else 30, caps=True)
    p.arc(13, 10, 7, 260, 350)
    p.poly((20, 6.5), (22, 12), (17, 10))


def star(p, f):
    pts = []
    for i in range(10):
        r = 9 if i % 2 == 0 else 4.2
        a = math.radians(-90 + i * 36)
        pts.append((12 + r * math.cos(a), 12.8 + r * math.sin(a)))
    if f:
        p.poly(*pts)
    p.line(*pts, pts[0])


def apps(p, f):
    for x, y in ((4, 4), (14, 4), (4, 14), (14, 14)):
        p.rrect(x, y, x + 6, y + 6, 2, fill=f)
        p.rrect(x, y, x + 6, y + 6, 2)


def transfer(p, f):
    p.line((8, 20), (8, 5))
    p.line((4, 9), (8, 5), (12, 9))
    p.line((16, 4), (16, 19))
    p.line((12, 15), (16, 19), (20, 15))


def stats(p, f):
    for x, h in ((5, 7), (12, 13), (19, 10)):
        p.rrect(x - 1.5, 20 - h, x + 1.5, 20, 1, fill=f)
        p.rrect(x - 1.5, 20 - h, x + 1.5, 20, 1)


def more(p, f):
    for x in (5, 12, 19):
        p.circle(x, 12, 1.2, fill=True)


def trash(p, f):
    p.line((4, 6), (20, 6))
    p.line((9, 6), (10, 3), (14, 3), (15, 6))
    p.rrect(6, 7, 18, 21, 2, fill=f)
    p.rrect(6, 7, 18, 21, 2)
    ink = 0 if f else 255
    p.line((10, 11), (10, 17), ink=ink)
    p.line((14, 11), (14, 17), ink=ink)


# ------------------------------------------------- the rest of the firmware's icons
def house(p, f):  # the original house shape, in this style
    p.line((3, 20.5), (3, 10), (12, 3), (21, 10), (21, 20.5), (3, 20.5))
    p.line((9.5, 20.5), (9.5, 14), (14.5, 14), (14.5, 20.5))


def folder(p, f):
    pts = [(3, 6), (9, 6), (11, 8.5), (21, 8.5), (21, 19), (3, 19)]
    if f:
        p.poly(*pts)
    p.line(*pts, pts[0])


def file_plain(p, f):
    pts = [(6, 3), (14, 3), (19, 8), (19, 21), (6, 21)]
    p.line(*pts, pts[0])
    p.line((14, 3), (14, 8), (19, 8))


def file_text(p, f):
    file_plain(p, f)
    for y in (13, 17):
        p.line((9.5, y), (15.5, y))


def history(p, f):
    p.circle(12, 12, 8.5)
    p.line((12, 7.5), (12, 12), (15, 14))


def sliders(p, f):
    for y, x in ((7, 15), (17, 9)):
        p.line((3, y), (21, y))
        p.circle(x, y, 2.6, fill=True)


def usb(p, f):
    p.line((12, 3), (12, 21))
    p.poly((12, 1.5), (14.5, 5), (9.5, 5))
    p.line((12, 15), (7, 11), (7, 8))
    p.line((12, 13), (17, 9.5), (17, 7.5))
    p.rrect(15.5, 5.5, 18.5, 8, 0.5, fill=True)
    p.circle(7, 7, 1.7, fill=True)
    p.circle(12, 19.5, 2.2, fill=True)


def book_open(p, f):
    p.line((12, 7.5), (9, 5), (2.5, 5), (2.5, 18), (9, 18), (12, 20.5))
    p.line((12, 7.5), (15, 5), (21.5, 5), (21.5, 18), (15, 18), (12, 20.5))
    p.line((12, 7.5), (12, 20.5))


def book_marked(p, f):
    book(p, False)
    p.line((10, 3), (10, 10.5), (12.5, 8.5), (15, 10.5), (15, 3))


def image(p, f):
    p.rrect(3, 4, 21, 20, 2)
    p.circle(8.5, 9, 1.6, fill=True)
    p.line((3.5, 18), (9.5, 12.5), (13.5, 16), (16.5, 13.5), (20.5, 17))


def landmark(p, f):
    p.line((3, 9), (12, 3.5), (21, 9), (3, 9))
    p.line((3, 20.5), (21, 20.5))
    for x in (6.5, 10.5, 13.5, 17.5):
        p.line((x, 12), (x, 17.5))


def radio_tower(p, f):
    p.circle(12, 9, 1.6, fill=True)
    for r, a in ((4.5, 50), (8.5, 45)):
        p.arc(12, 9, r, 180 - a, 180 + a)
        p.arc(12, 9, r, -a, a)
    p.line((8, 21), (12, 12), (16, 21))


def send(p, f):
    p.line((3, 4), (21, 12), (3, 20), (6.5, 12), (3, 4))
    p.line((6.5, 12), (13, 12))


def chevron_up(p, f):
    p.line((6, 15), (12, 9), (18, 15))


def chevron_down(p, f):
    p.line((6, 9), (12, 15), (18, 9))


def double_chevron(dx, dy):
    def draw(p, f):
        for k in (-3, 3):
            cx, cy = 12 + dx * k, 12 + dy * k
            ax, ay = -dy * 5, dx * 5  # the arms run across the direction
            p.line((cx - dx * 3 + ax, cy - dy * 3 + ay), (cx + dx * 2, cy + dy * 2), (cx - dx * 3 - ax, cy - dy * 3 - ay))
    return draw


def arrow_up(p, f):
    p.line((12, 20), (12, 4.5))
    p.line((6, 10.5), (12, 4.5), (18, 10.5))


def arrow_down(p, f):
    p.line((12, 4), (12, 19.5))
    p.line((6, 13.5), (12, 19.5), (18, 13.5))


def sort(down_wide):
    def draw(p, f):
        if down_wide:
            p.line((6, 4), (6, 20))
            p.line((3, 16.5), (6, 20), (9, 16.5))
            bars = (21, 18, 15)
        else:
            p.line((6, 20), (6, 4))
            p.line((3, 7.5), (6, 4), (9, 7.5))
            bars = (15, 18, 21)
        for y, x in zip((6, 12, 18), bars):
            p.line((12, y), (x, y))
    return draw


def more_vertical(p, f):
    for y in (5, 12, 19):
        p.circle(12, y, 1.3, fill=True)


def code_dots(p, f):
    p.line((6.5, 7.5), (2.5, 12), (6.5, 16.5))
    p.line((17.5, 7.5), (21.5, 12), (17.5, 16.5))
    for x in (8.5, 12, 15.5):
        p.circle(x, 12, 1.2, fill=True)


def menu(p, f):
    for y in (6, 12, 18):
        p.line((4, y), (20, y))


def align_start(p, f):
    for y, x in ((6, 21), (12, 14), (18, 18)):
        p.line((3, y), (x, y))


def case_sensitive(p, f):  # "Aa"
    p.line((2.5, 18.5), (7, 6), (11.5, 18.5))
    p.line((4.3, 14), (9.7, 14))
    p.circle(17, 15, 3.2)
    p.line((20.2, 11.5), (20.2, 18.5))


def backspace(p, f):
    p.line((21, 5), (8, 5), (2.5, 12), (8, 19), (21, 19), (21, 5))
    p.line((12, 9.5), (17, 14.5))
    p.line((17, 9.5), (12, 14.5))


def tablet(p, f):
    p.rrect(5, 2.5, 19, 21.5, 2.5)
    p.line((10.5, 18), (13.5, 18))


def slashed(fn):
    def draw(p, f):
        fn(p, False)
        p.line((3, 3), (21, 21), sw=5, ink=0)
        p.line((3, 3), (21, 21))
    return draw


# Every symbol in src/components/icons (name without icon_ and size) -> drawing.
SYMBOLS = {
    "folder": folder, "file_text": file_text, "image": image, "book_open": book_open, "book": book,
    "book_marked": book_marked, "file": file_plain, "wifi": wifi, "lyra_library": library, "landmark": landmark,
    "radio_tower": radio_tower, "history": history, "lyra_settings": gear, "cog": gear,
    "case_sensitive": case_sensitive, "text_align_start": align_start, "bookmark": bookmark, "ellipsis": more,
    "transfer": transfer, "usb": usb, "lyra_transfer": send, "chevrons_left_right_ellipsis": code_dots, "menu": menu,
    "home": house, "search": search, "chevron_up": chevron_up, "chevron_down": chevron_down, "arrow_up": arrow_up,
    "arrow_down": arrow_down, "sliders_horizontal": sliders, "arrows_up": double_chevron(0, -1),
    "arrows_down": double_chevron(0, 1), "arrows_left": double_chevron(-1, 0), "arrows_right": double_chevron(1, 0),
    "arrow_down_wide_narrow": sort(True), "arrow_up_narrow_wide": sort(False), "refresh_cw": refresh,
    "ellipsis_vertical": more_vertical, "reading_stats": stats, "backspace": backspace, "lightbulb": bulb,
    "lightbulb_off": slashed(bulb), "tabler_lock": lock, "tabler_moon_filled": moon, "back": back,
    "device_tablet": tablet, "device_tablet_off": slashed(tablet), "apps": apps,
}


ICONS = [
    ("Control Center", [("wifi", wifi), ("wifi_off", wifi_off), ("contrast", contrast), ("refresh", refresh),
                        ("lock", lock), ("lock_open", lock_open), ("sync", sync), ("power", power),
                        ("gear", gear), ("sun", sun), ("moon", moon), ("snowflake", snowflake), ("flame", flame), ("sunset", sunset), ("candle", candle), ("thermo_hot", thermo_hot), ("bulb", bulb)]),
    ("Navigation", [("home", home), ("back", back), ("close", close), ("minus", minus), ("plus", plus), ("search", search), ("more", more),
                    ("apps", apps), ("transfer", transfer)]),
    ("Reading", [("book", book), ("library", library), ("bookmark", bookmark), ("highlight", highlight),
                 ("dictionary", dictionary), ("text_size", text_size), ("toc", toc), ("rotate", rotate),
                 ("star", star), ("stats", stats), ("trash", trash)]),
]


def render(fn, filled, size=24):
    p = Pen(size)
    fn(p, filled)
    return p.bitmap()



def header_bytes(bm):
    """Rows MSB-first, padded to whole bytes; 1 = white, 0 = ink (freeink::Icon)."""
    w, h = bm.size
    px = bm.load()
    out = []
    for y in range(h):
        for xb in range(0, w, 8):
            v = 0
            for b in range(8):
                x = xb + b
                white = x >= w or px[x, y] != 0
                v = (v << 1) | (1 if white else 0)
            out.append(v)
    return out


# Icons the firmware uses, with the states it needs.
FIRMWARE = ["wifi", "contrast", "refresh", "lock", "sync", "power", "gear", "snowflake", "flame:on", "bulb:on",
            "bulb", "moon", "dictionary", "more", "text_size", "stats", "library", "apps", "transfer", "rotate", "minus", "plus"]


def write_header(path):
    fns = {n: fn for _, g in ICONS for n, fn in g}
    lines = ["#pragma once", "", '#include "Icon.h"', "",
             "// VARAGH icon system (1.3.0 Lion). Generated by scripts/generate_varagh_icons.py. Do not edit.",
             "// 24 px grid, 2 px strokes on whole pixels, 1-bit. *_on = filled (selected) state.", ""]
    for entry in FIRMWARE:
        name, _, state = entry.partition(":")
        filled = state == "on"
        sym = f"icon_v_{name}{'_on' if filled else ''}_24"
        data = header_bytes(render(fns[name], filled))
        rows = [", ".join(f"0x{b:02X}" for b in data[i:i + 18]) for i in range(0, len(data), 18)]
        lines.append(f"static const uint8_t {sym}_bits[] = {{")
        lines += ["    " + r + ("," if i < len(rows) - 1 else "};") for i, r in enumerate(rows)]
        lines.append(f"static const freeink::Icon {sym} = {{24, 24, {optical_centre(render(fns[name], filled))}, {sym}_bits}};")
        lines.append("")
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines))


def optical_centre(bm):
    """Row of the artwork's centre of mass (freeink::Icon opticalCenterY)."""
    w, h = bm.size
    px = bm.load()
    rows = [y for y in range(h) for x in range(w) if px[x, y] == 0]
    return round(sum(rows) / len(rows)) if rows else h // 2


def icon_lines(sym, bm):
    data = header_bytes(bm)
    w, h = bm.size
    per = (w + 7) // 8 * 6
    rows = [", ".join(f"0x{b:02X}" for b in data[i:i + per]) for i in range(0, len(data), per)]
    out = [f"static const uint8_t {sym}_bits[] = {{"]
    out += ["    " + r + ("," if i < len(rows) - 1 else "};") for i, r in enumerate(rows)]
    out.append(f"static const freeink::Icon {sym} = {{{w}, {h}, {optical_centre(bm)}, {sym}_bits}};")
    return out


def rewrite_headers(icon_dir):
    """Redraw every freeink::Icon in icon_dir, keeping each header's symbols and sizes."""
    for path in sorted(icon_dir.glob("*.h")):
        if path.name == "varaghIcons.h":
            continue
        text = path.read_text(encoding="utf-8")
        syms = re.findall(r"static const freeink::Icon (icon_\w+?)_(\d+) = ", text)
        if path.name == "book24.h":
            bits = header_bytes(render(book, False, 24))
            rows = [", ".join(f"0x{b:02X}" for b in bits[i:i + 18]) for i in range(0, len(bits), 18)]
            body = ["#pragma once", "#include <cstdint>", "",
                    "// Generated by scripts/generate_varagh_icons.py (VARAGH style B). Do not edit.",
                    "// size: 24x24", "static const uint8_t Book24Icon[] = {"]
            body += ["    " + r + ("," if i < len(rows) - 1 else "};") for i, r in enumerate(rows)]
            path.write_text("\n".join(body) + "\n", encoding="utf-8", newline="\n")
            continue
        if not syms:
            continue  # pictures (cover, clock faces), not icons
        lines = ["#pragma once", "", '#include "Icon.h"', "",
                 "// Generated by scripts/generate_varagh_icons.py (VARAGH style B). Do not edit.", ""]
        for base, size in syms:
            name = base[len("icon_"):]
            if name not in SYMBOLS:
                sys.exit(f"{path.name}: no drawing for {name}")
            lines += icon_lines(f"{base}_{size}", render(SYMBOLS[name], False, int(size)))
            lines.append("")
        path.write_text("\n".join(lines), encoding="utf-8", newline="\n")
        print(f"{path.name}: {len(syms)} icons")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    icons = Path(__file__).resolve().parent.parent / "src/components/icons"
    ap.add_argument("--header", default=str(icons / "varaghIcons.h"))
    write_header(ap.parse_args().header)
    rewrite_headers(icons)

