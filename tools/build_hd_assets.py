#!/usr/bin/env python3
"""Generate 2x sprite sheets for the remake (requires Pillow + numpy).

The source SWF only contains low-resolution bitmaps, so these are
high-quality Lanczos enlargements with a light unsharp mask. They are
rendered into a higher-density canvas to keep the game crisp on HiDPI
screens.
"""
import os

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src", "assets", "img")
DST = os.path.join(ROOT, "src", "assets", "img@2x")
SCALE = 2


def upscale(src, dst, sharpen=True):
    im = Image.open(src).convert("RGBA")
    w, h = im.size
    size = (w * SCALE, h * SCALE)

    # Resize premultiplied so transparent pixels do not bleed dark edges.
    arr = np.asarray(im).astype(np.float32)
    alpha = arr[..., 3:4] / 255.0
    pre = arr.copy()
    pre[..., :3] *= alpha
    pre_img = Image.fromarray(np.clip(pre, 0, 255).astype(np.uint8), "RGBA")
    pre_img = pre_img.resize(size, Image.Resampling.LANCZOS)

    if sharpen:
        r, g, b, a = pre_img.split()
        rgb = Image.merge("RGB", (r, g, b)).filter(
            ImageFilter.UnsharpMask(radius=1.1, percent=45, threshold=2))
        pre_img = Image.merge("RGBA", (*rgb.split(), a))

    out = np.asarray(pre_img).astype(np.float32)
    a = out[..., 3:4] / 255.0
    out[..., :3] = np.where(a > 1e-4, out[..., :3] / np.maximum(a, 1e-4), 0)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGBA").save(
        dst, optimize=True)


def main():
    count = 0
    for root, _, files in os.walk(SRC):
        rel = os.path.relpath(root, SRC)
        for fn in sorted(files):
            if not fn.lower().endswith(".png"):
                continue
            src = os.path.join(root, fn)
            dst = os.path.join(DST, rel, fn)
            upscale(src, dst, sharpen=rel.split(os.sep)[0] != "bg")
            count += 1
    size = sum(os.path.getsize(os.path.join(r, f))
               for r, _, fs in os.walk(DST) for f in fs)
    print("wrote %d sprites to %s (%.2f MB)" % (count, DST, size / 1048576.0))


if __name__ == "__main__":
    main()
