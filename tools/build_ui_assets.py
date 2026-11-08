#!/usr/bin/env python3
"""Build the 3DS title/menu art that is not taken from the game's sprite sets.

Generates, in ports/3ds/gfx/:

    btn.png / btnSel.png   glossy pink menu buttons (96x30 display size, 4x render)
    heroBg.png             light-blue bubble background for character select

and, when --poster is given, re-crops the original homepage screenshot into
title.png: the screenshot has the game canvas centred on a grey page, so the
canvas is detected first, then cover-cropped to the 5:3 screens and resized.

    python3 -I tools/build_ui_assets.py
    python3 -I tools/build_ui_assets.py --poster /path/to/homepage.png
"""

import argparse
import math
import os
import random
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GFX = os.path.join(ROOT, "ports", "3ds", "gfx")

TITLE_W, TITLE_H = 400, 240


# ---------------------------------------------------------------- PNG codec

def read_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    pos, idat = 8, b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            w, h, depth, color = struct.unpack(">IIBB", body[:10])
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    assert depth == 8 and color in (2, 6), (depth, color)
    nch = 4 if color == 6 else 3
    stride = w * nch
    raw = zlib.decompress(idat)
    out = bytearray(h * stride)
    prev = bytearray(stride)
    pos = 0
    for y in range(h):
        ftype = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        if ftype == 1:
            for i in range(nch, stride):
                line[i] = (line[i] + line[i - nch]) & 255
        elif ftype == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif ftype == 3:
            for i in range(stride):
                a = line[i - nch] if i >= nch else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
        elif ftype == 4:
            for i in range(stride):
                a = line[i - nch] if i >= nch else 0
                b = prev[i]
                c = prev[i - nch] if i >= nch else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, nch, out


def write_png(path, w, h, nch, pixels):
    stride = w * nch
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += pixels[y * stride:(y + 1) * stride]
    color = 6 if nch == 4 else 2

    def chunk(kind, body):
        head = struct.pack(">I", len(body)) + kind + body
        return head + struct.pack(">I", zlib.crc32(kind + body) & 0xffffffff)

    blob = (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, color, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))
    open(path, "wb").write(blob)


# ---------------------------------------------------------------- poster

def box_resize(w, h, nch, px, x0, y0, cw, ch, out_w, out_h):
    """Box-average a source rectangle down to out_w x out_h."""
    out = bytearray(out_w * out_h * 3)
    for dy in range(out_h):
        sy0 = y0 + dy * ch / out_h
        sy1 = y0 + (dy + 1) * ch / out_h
        for dx in range(out_w):
            sx0 = x0 + dx * cw / out_w
            sx1 = x0 + (dx + 1) * cw / out_w
            r = g = b = n = 0
            for sy in range(int(sy0), max(int(sy0) + 1, int(math.ceil(sy1)))):
                for sx in range(int(sx0), max(int(sx0) + 1, int(math.ceil(sx1)))):
                    i = (sy * w + sx) * nch
                    r += px[i]
                    g += px[i + 1]
                    b += px[i + 2]
                    n += 1
            o = (dy * out_w + dx) * 3
            out[o] = r // n
            out[o + 1] = g // n
            out[o + 2] = b // n
    return out_w, out_h, 3, out


def poster_crop(src):
    """Detect the game canvas (the grey page around it) and fit it to 400x240."""
    w, h, nch, px = read_png(src)

    def is_page(x, y):
        i = (y * w + x) * nch
        r, g, b = px[i], px[i + 1], px[i + 2]
        return abs(r - g) < 6 and abs(g - b) < 6 and 55 < r < 95

    mid = h // 2
    xs = [x for x in range(w) if not is_page(x, mid)]
    x0, x1 = min(xs), max(xs) + 1
    cw = x1 - x0
    # The screenshot is cropped vertically, so the canvas is 11:8 wide.
    ch = min(h, int(round(cw * 8 / 11.0)))
    y0 = (h - ch) // 2

    # Cover-crop the canvas to the screen aspect, then box-average down.
    if cw / ch > TITLE_W / TITLE_H:
        keep_w = int(round(ch * TITLE_W / TITLE_H))
        cx = x0 + (cw - keep_w) // 2
        y0, ch, x0, cw = y0, ch, cx, keep_w
    else:
        keep_h = int(round(cw * TITLE_H / TITLE_W))
        y0 += (ch - keep_h) // 2
        ch = keep_h

    return box_resize(w, h, nch, px, x0, y0, cw, ch, TITLE_W, TITLE_H)


def title_fit(src):
    """Fit a clean 550x400 game canvas to 400x240, dropping the grey info bar.

    The canvas is drawn with a screenshot's status strip (画质/音乐) along the
    bottom; it is chrome from the page, not part of the art, so it is cut off
    before the (slightly non-uniform) fit that keeps the whole composition."""
    w, h, nch, px = read_png(src)

    def is_bar(y):
        n = 0
        total = 0
        for x in range(0, w, 4):
            i = (y * w + x) * nch
            total += 1
            if (abs(px[i] - 102) <= 12 and abs(px[i + 1] - 92) <= 12
                    and abs(px[i + 2] - 102) <= 12):
                n += 1
        return n > total * 0.10

    bars = [y for y in range(h * 3 // 4, h) if is_bar(y)]
    bottom = bars[0] if bars else h
    return box_resize(w, h, nch, px, 0, 0, w, bottom, TITLE_W, TITLE_H)


# ---------------------------------------------------------------- buttons

BTN_W, BTN_H = 96, 30
SS = 4  # supersampling factor


def rounded_rect(px, w, h, x0, y0, x1, y1, radius, color):
    r, g, b, a = color
    for y in range(max(0, int(y0)), min(h, int(math.ceil(y1)))):
        for x in range(max(0, int(x0)), min(w, int(math.ceil(x1)))):
            cx = min(max(x, x0 + radius), x1 - radius)
            cy = min(max(y, y0 + radius), y1 - radius)
            if (x - cx) ** 2 + (y - cy) ** 2 <= radius * radius:
                blend(px, w, x, y, (r, g, b, a))


def blend(px, w, x, y, color):
    r, g, b, a = color
    i = (y * w + x) * 4
    sa = a / 255.0
    da = px[i + 3] / 255.0
    oa = sa + da * (1 - sa)
    if oa <= 0:
        return
    for k, sc in enumerate((r, g, b)):
        dc = px[i + k]
        px[i + k] = int(round((sc * sa + dc * da * (1 - sa)) / oa))
    px[i + 3] = int(round(oa * 255))


def make_button(selected):
    w, h = BTN_W * SS, BTN_H * SS
    px = bytearray(w * h * 4)
    radius = 13 * SS
    margin = 1 * SS
    top = (255, 176, 214) if selected else (255, 155, 203)
    bottom = (247, 62, 150) if selected else (228, 44, 122)
    border = (255, 214, 102) if selected else (255, 255, 255)
    border_a = 235 if selected else 190

    # Border capsule first; the gradient body is drawn inset on top of it.
    rounded_rect(px, w, h, 0, 0, w, h, radius + 2 * SS, border + (border_a,))
    for y in range(h):
        t = y / (h - 1)
        body = tuple(int(round(top[k] + (bottom[k] - top[k]) * t)) for k in range(3))
        rounded_rect(px, w, h, margin, margin, w - margin, h - margin, radius,
                     body + (255,))

    # Glossy highlight across the top third.
    gloss = (255, 255, 255, 60)
    rounded_rect(px, w, h, margin + 5 * SS, margin + 3 * SS,
                 w - margin - 5 * SS, margin + 11 * SS, 7 * SS, gloss)

    # Box-downsample to 96x30.
    out = bytearray(BTN_W * BTN_H * 4)
    for y in range(BTN_H):
        for x in range(BTN_W):
            r = g = b = a = 0
            for sy in range(SS):
                for sx in range(SS):
                    i = ((y * SS + sy) * w + x * SS + sx) * 4
                    r += px[i]
                    g += px[i + 1]
                    b += px[i + 2]
                    a += px[i + 3]
            n = SS * SS
            o = (y * BTN_W + x) * 4
            out[o] = r // n
            out[o + 1] = g // n
            out[o + 2] = b // n
            out[o + 3] = a // n
    return BTN_W, BTN_H, 4, out


# ---------------------------------------------------------------- hero background

def make_hero_bg():
    ss = 2
    w, h = TITLE_W * ss, TITLE_H * ss
    px = bytearray(w * h * 4)
    top = (216, 247, 255)
    bottom = (130, 208, 243)
    for y in range(h):
        t = y / (h - 1)
        c = tuple(int(round(top[k] + (bottom[k] - top[k]) * t)) for k in range(3))
        for x in range(w):
            i = (y * w + x) * 4
            px[i], px[i + 1], px[i + 2], px[i + 3] = c[0], c[1], c[2], 255

    rnd = random.Random(7)
    for _ in range(46):
        cx = rnd.uniform(0, w)
        cy = rnd.uniform(0, h)
        rad = rnd.uniform(9, 26) * ss
        thickness = 2 * ss
        for y in range(int(cy - rad - thickness), int(cy + rad + thickness) + 1):
            if y < 0 or y >= h:
                continue
            for x in range(int(cx - rad - thickness), int(cx + rad + thickness) + 1):
                if x < 0 or x >= w:
                    continue
                d = math.hypot(x - cx, y - cy)
                if abs(d - rad) <= thickness / 2:
                    blend(px, w, x, y, (255, 255, 255, 70))

    out = bytearray(TITLE_W * TITLE_H * 4)
    for y in range(TITLE_H):
        for x in range(TITLE_W):
            r = g = b = a = 0
            for sy in range(ss):
                for sx in range(ss):
                    i = ((y * ss + sy) * w + x * ss + sx) * 4
                    r += px[i]
                    g += px[i + 1]
                    b += px[i + 2]
                    a += px[i + 3]
            n = ss * ss
            o = (y * TITLE_W + x) * 4
            out[o] = r // n
            out[o + 1] = g // n
            out[o + 2] = b // n
            out[o + 3] = a // n
    return TITLE_W, TITLE_H, 4, out


def write_t3s(name):
    with open(os.path.join(GFX, name + ".t3s"), "w") as f:
        f.write("--atlas -f rgba8888\n%s.png\n" % name)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--poster", help="homepage screenshot (PNG) to re-crop into title.png")
    ap.add_argument("--title", help="clean title-screen canvas (PNG) to fit into title.png")
    args = ap.parse_args()

    for name, data in (("btn", make_button(False)), ("btnSel", make_button(True)),
                       ("heroBg", make_hero_bg())):
        write_png(os.path.join(GFX, name + ".png"), *data)
        write_t3s(name)
        print("wrote %s.png %dx%d" % (name, data[0], data[1]))

    if args.poster:
        data = poster_crop(args.poster)
        write_png(os.path.join(GFX, "title.png"), *data)
        print("wrote title.png %dx%d" % (data[0], data[1]))

    if args.title:
        data = title_fit(args.title)
        write_png(os.path.join(GFX, "title.png"), *data)
        print("wrote title.png %dx%d" % (data[0], data[1]))


if __name__ == "__main__":
    main()
