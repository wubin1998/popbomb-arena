#!/usr/bin/env python3
"""Build the 3DS bitmap font atlases used for the Chinese UI.

Scans ports/3ds/source/*.c and *.h for every non-ASCII character (plus printable
ASCII) and renders it at each of the sizes the UI actually uses, into one RGBA
atlas per size:

    ports/3ds/gfx/zh.png          24px glyphs, headings
    ports/3ds/gfx/zh16.png        16px glyphs, body text
    ports/3ds/gfx/zh*.t3s         tex3ds descriptions (la8: white mask + alpha)
    ports/3ds/source/zh_glyphs.h  glyph tables consumed by font.c

Glyphs are drawn at their native size and blitted 1:1. Scaling a single 26px
atlas down to 10-16px is what made the UI look blurred on hardware.

Run from anywhere:

    python3 -I tools/build_zh_font.py [--font path/to/NotoSansSC-Regular.ttf]

Noto Sans SC is licensed under the SIL Open Font License 1.1.
"""

import argparse
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(ROOT, "ports", "3ds", "source")
GFX_DIR = os.path.join(ROOT, "ports", "3ds", "gfx")

# One entry per native size. CELL is the glyph slot, COLS the row width; the
# atlas is padded up to powers of two.
SIZES = [
    {"name": "zh", "px": 24, "cell": 30, "cols": 34},
    {"name": "zh16", "px": 16, "cell": 22, "cols": 46},
]

# Padding around each glyph's ink box, so antialiased edges are not clipped.
PAD = 1


def collect_codepoints():
    cps = set(range(32, 127))
    for name in sorted(os.listdir(SRC_DIR)):
        if not name.endswith((".c", ".h")):
            continue
        with open(os.path.join(SRC_DIR, name), "r", encoding="utf-8") as f:
            text = f.read()
        for ch in text:
            if ord(ch) > 127:
                cps.add(ord(ch))
    return sorted(cps)


def pow2_at_least(v):
    return 1 << max(0, (v - 1).bit_length())


def build_size(font_path, cps, spec):
    px, cell, cols = spec["px"], spec["cell"], spec["cols"]
    font = ImageFont.truetype(font_path, px)

    rows = math.ceil(len(cps) / cols)
    width = pow2_at_least(cols * cell)
    height = pow2_at_least(rows * cell)

    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    glyphs = []

    for idx, cp in enumerate(cps):
        ch = chr(cp)
        x0, y0, x1, y1 = font.getbbox(ch)
        cw, chh = x1 - x0, y1 - y0
        adv = font.getlength(ch)
        cell_x = (idx % cols) * cell
        cell_y = (idx // cols) * cell

        if cw <= 0 or chh <= 0:
            glyphs.append((cp, 0, 0, 0, 0, 0, 0, adv))
            continue
        if cw + 2 * PAD > cell or chh + 2 * PAD > cell:
            sys.exit("glyph %r is %dx%d, does not fit a %dpx cell" % (ch, cw, chh, cell))

        mx = (cell - cw) // 2
        my = (cell - chh) // 2
        draw.text((cell_x + mx - x0, cell_y + my - y0), ch, font=font, fill=(255, 255, 255, 255))

        gx, gy = cell_x + mx - PAD, cell_y + my - PAD
        gw, gh = cw + 2 * PAD, chh + 2 * PAD
        glyphs.append((cp, gx, gy, gw, gh, x0 - PAD, y0 - PAD, adv))

    img_path = os.path.join(GFX_DIR, spec["name"] + ".png")
    img.save(img_path)

    # la8 keeps the glyph mask plus its alpha at half the size of rgba8888; the
    # glyphs are white, so the tint supplies the colour.
    with open(os.path.join(GFX_DIR, spec["name"] + ".t3s"), "w", encoding="utf-8") as f:
        f.write("--atlas -f la8\n%s.png\n" % spec["name"])

    return glyphs, width, height, px


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--font", default=os.path.join(ROOT, "tools", "fonts", "NotoSansSC-subset.ttf"))
    ap.add_argument("--subset-out", help="write a font subset covering the collected characters")
    ap.add_argument("--list", action="store_true", help="print the collected characters and exit")
    args = ap.parse_args()

    cps = collect_codepoints()
    if args.list:
        print("".join(chr(c) for c in cps if c > 127))
        return 0

    if not os.path.exists(args.font):
        sys.exit("font not found: %s" % args.font)

    if args.subset_out:
        from fontTools import subset

        opts = subset.Options()
        opts.layout_features = ["*"]
        opts.name_IDs = ["*"]
        opts.notdef_outline = True
        opts.recalc_bounds = True
        full = subset.load_font(args.font, opts)
        sub = subset.Subsetter(opts)
        sub.populate(unicodes=cps)
        sub.subset(full)
        subset.save_font(full, args.subset_out, opts)
        print("wrote %s (%d glyphs)" % (args.subset_out, len(cps)))

    os.makedirs(GFX_DIR, exist_ok=True)

    built = []
    for spec in SIZES:
        glyphs, width, height, px = build_size(args.font, cps, spec)
        built.append((spec, glyphs, width, height, px))
        print("wrote %s.png %dx%d (%d glyphs, %dpx)"
              % (spec["name"], width, height, len(glyphs), px))

    header = os.path.join(SRC_DIR, "zh_glyphs.h")
    with open(header, "w", encoding="utf-8") as f:
        f.write("/* Generated by tools/build_zh_font.py -- do not edit. */\n")
        f.write("#pragma once\n\n#include <stdint.h>\n\n")
        f.write("#define ZH_FONT_COUNT %d\n\n" % len(built))
        f.write("typedef struct {\n")
        f.write("\tuint32_t cp;\n")
        f.write("\tuint16_t x, y, w, h;\n")
        f.write("\tint16_t ox, oy;\n")
        f.write("\tfloat adv;\n")
        f.write("} ZhGlyph;\n\n")
        f.write("typedef struct {\n")
        f.write("\tconst ZhGlyph* glyphs;\n")
        f.write("\tint count;\n")
        f.write("\tint atlas_w, atlas_h;\n")
        f.write("\tint px;\n")
        f.write("} ZhFontDef;\n\n")

        for i, (spec, glyphs, width, height, px) in enumerate(built):
            f.write("static const ZhGlyph ZH_GLYPHS_%d[] = {\n" % i)
            for cp, gx, gy, gw, gh, ox, oy, adv in glyphs:
                f.write("\t{ %d, %d, %d, %d, %d, %d, %d, %.1ff },\n"
                        % (cp, gx, gy, gw, gh, ox, oy, adv))
            f.write("};\n\n")

        f.write("static const ZhFontDef ZH_FONTS[ZH_FONT_COUNT] = {\n")
        for i, (spec, glyphs, width, height, px) in enumerate(built):
            f.write("\t{ ZH_GLYPHS_%d, (int)(sizeof(ZH_GLYPHS_%d) / sizeof(ZhGlyph)), %d, %d, %d },\n"
                    % (i, i, width, height, px))
        f.write("};\n")

    print("wrote %s" % header)
    return 0


if __name__ == "__main__":
    sys.exit(main())
