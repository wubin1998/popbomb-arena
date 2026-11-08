#!/usr/bin/env python3
"""Generate the 3DS CIA icon (48x48) and home-menu banner (256x128)."""

import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "ports", "3ds", "cia")
FONT = os.path.join(ROOT, "tools", "fonts", "NotoSansSC-subset.ttf")


def gradient(size, top, bottom):
    w, h = size
    img = Image.new("RGBA", size)
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        d.line([(0, y), (w, y)], fill=c + (255,))
    return img


def scaled(path, height):
    im = Image.open(path).convert("RGBA")
    w = round(im.width * height / im.height)
    return im.resize((w, height), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)

    # Icon: bomb on a dark gradient tile.
    icon = gradient((48, 48), (32, 44, 78), (10, 12, 20))
    bomb = scaled(os.path.join(ROOT, "src/assets/img/bomb/1.png"), 36)
    icon.alpha_composite(bomb, ((48 - bomb.width) // 2, (48 - bomb.height) // 2 + 1))
    icon.save(os.path.join(OUT, "icon.png"))

    # Banner: title on the left, the four heroes on the right.
    ban = gradient((256, 128), (34, 48, 84), (9, 11, 19))
    d = ImageDraw.Draw(ban)
    d.rectangle([0, 0, 255, 127], outline=(90, 110, 156, 255), width=2)

    title_font = ImageFont.truetype(FONT, 30)
    sub_font = ImageFont.truetype(FONT, 15)
    d.text((16, 30), "泡泡堂", font=title_font, fill=(255, 255, 255, 255))
    d.text((16, 66), "大乱斗", font=title_font, fill=(255, 209, 102, 255))
    d.text((17, 104), "POPBOMB ARENA", font=sub_font, fill=(150, 165, 195, 255))

    for i in range(4):
        hero = scaled(os.path.join(ROOT, "src/assets/img/hero%d/4.png" % (i + 1)), 54)
        ban.alpha_composite(hero, (128 + i * 28, 62 - (i % 2) * 4))

    ban.convert("RGB").save(os.path.join(OUT, "banner.png"))
    print("wrote", os.path.join(OUT, "icon.png"))
    print("wrote", os.path.join(OUT, "banner.png"))


if __name__ == "__main__":
    main()
