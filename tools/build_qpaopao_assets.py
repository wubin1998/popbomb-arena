#!/usr/bin/env python3
"""Build web assets for Qpaopaotang.swf (run with python3 -I)."""
import os
import shutil
import struct
import zlib

ROOT = "/Users/wubin/Desktop/Application/flash-game"
SPR = os.path.join(ROOT, "analysis/qpaopaotang_sprites")
SND = os.path.join(ROOT, "analysis/qpaopaotang_sounds")
OUT = os.path.join(ROOT, "src/assets")

SETS = {
    "bg": ("DefineSprite_232_bg", 12),
    "brick": ("DefineSprite_196_brick", 58),
    "tool": ("DefineSprite_294", 12),
    "bomb": ("DefineSprite_372_bomb", 40),
    "exp": ("DefineSprite_274_exp", 12),
    "brickExp": ("DefineSprite_249_brickExp", 11),
    "heroExp": ("DefineSprite_238_heroExp", 36),
    "dieSmoke": ("DefineSprite_280_heroDieSmoke", 36),
    "rocket": ("DefineSprite_306_rocket", 4),
    "rocketSmoke": ("DefineSprite_314_rocketSmoke", 4),
    "mine": ("DefineSprite_307_mine", 60),
    "grenade": ("DefineSprite_311_grenade", 4),
    "gas": ("DefineSprite_316_gasBomb", 101),
    "fireEffect": ("DefineSprite_320_fireEffect", 40),
    "freezeEffect": ("DefineSprite_326_freezeEffect", 40),
    "pointer": ("DefineSprite_211_p_pointer", 60),
}

HEROES = {
    "hero1": "DefineSprite_91_hero1",
    "hero2": "DefineSprite_69_hero2",
    "hero3": "DefineSprite_47_hero3",
    "hero4": "DefineSprite_25_hero4",
}

SOUNDS = {
    "390_bibi.wav.mp3": "mine",
    "391_Bombit_-_level_1.mp3": "bgm1",
    "392_Bombit_-_level_2.mp3": "bgm2",
    "393_Bombit_-_level_3.mp3": "bgm3",
    "394_Bombit_-_level_4.mp3": "bgm4",
    "395_Bombit_-_level_complete.mp3": "stageWin",
    "396_Bombit_-_mainscreen.mp3": "menu",
    "397_Bombit_-_mission_failed.mp3": "stageLose",
    "398_exp.wav.mp3": "explode",
    "399_grenade.mp3.mp3": "grenade",
    "400_pushBomb.mp3.mp3": "push",
    "401_putBomb.wav.mp3": "place",
    "402_rocket.mp3.mp3": "rocket",
    "403_tool.wav.mp3": "pickup",
    "315.mp3": "sfx315",
    "319.mp3": "sfx319",
    "325.mp3": "sfx325",
}


def read_png(path):
    d = open(path, "rb").read()
    o = 8
    w = h = None
    idat = b""
    while o < len(d):
        ln = struct.unpack(">I", d[o:o + 4])[0]
        typ = d[o + 4:o + 8]
        chunk = d[o + 8:o + 8 + ln]
        o += 12 + ln
        if typ == b"IHDR":
            w, h, bd, ct = struct.unpack(">IIBB", chunk[:10])
            if bd != 8 or ct != 6:
                return None
        elif typ == b"IDAT":
            idat += chunk
        elif typ == b"IEND":
            break
    raw = zlib.decompress(idat)
    stride = w * 4
    prev = bytearray(stride)
    rows = []
    p = 0
    for _ in range(h):
        f = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0
            b = prev[i]
            c = prev[i - 4] if i >= 4 else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + ((a + b) // 2)) & 255
            elif f == 4:
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        rows.append(line)
        prev = line
    return w, h, rows


def write_png(path, w, h, rows):
    raw = bytearray()
    for row in rows:
        raw.append(0)
        raw.extend(row)

    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


def trim(path, pad=0):
    got = read_png(path)
    if not got:
        return
    w, h, rows = got
    xs, ys = [], []
    for y in range(h):
        for x in range(w):
            if rows[y][x * 4 + 3] > 6:
                xs.append(x)
                ys.append(y)
    if not xs:
        return
    x0, x1 = max(0, min(xs) - pad), min(w - 1, max(xs) + pad)
    y0, y1 = max(0, min(ys) - pad), min(h - 1, max(ys) + pad)
    nw, nh = x1 - x0 + 1, y1 - y0 + 1
    if (nw, nh) == (w, h):
        return
    out = [row[x0 * 4:(x1 + 1) * 4] for row in rows[y0:y1 + 1]]
    write_png(path, nw, nh, out)


def reset():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    os.makedirs(OUT)


def copies(name, count, ext="png"):
    src = os.path.join(SPR, SETS[name][0])
    dst = os.path.join(OUT, "img", name)
    os.makedirs(dst, exist_ok=True)
    got = 0
    for i in range(1, count + 1):
        p = os.path.join(src, "%d.png" % i)
        if os.path.exists(p):
            shutil.copyfile(p, os.path.join(dst, "%d.png" % i))
            got += 1
    return got


def main():
    reset()
    total = {}
    for name, (_, n) in SETS.items():
        total[name] = copies(name, n)
    for name, src in HEROES.items():
        dst = os.path.join(OUT, "img", name)
        os.makedirs(dst, exist_ok=True)
        for i in range(1, 5):
            p = os.path.join(SPR, src, "%d.png" % i)
            if os.path.exists(p):
                shutil.copyfile(p, os.path.join(dst, "%d.png" % i))
        total[name] = 4
    os.makedirs(os.path.join(OUT, "audio"), exist_ok=True)
    for fn, name in SOUNDS.items():
        p = os.path.join(SND, fn)
        if os.path.exists(p):
            shutil.copyfile(p, os.path.join(OUT, "audio", name + ".mp3"))
    # Trim transparent padding so every sprite can be centred on a tile.
    for root, _, files in os.walk(os.path.join(OUT, "img")):
        if os.path.basename(root) == "bg":
            continue
        for fn in files:
            if fn.endswith(".png"):
                trim(os.path.join(root, fn), pad=1)
    print(total)
    size = 0
    for root, _, files in os.walk(OUT):
        size += sum(os.path.getsize(os.path.join(root, f)) for f in files)
    print("assets: %.2f MB" % (size / 1048576.0))


if __name__ == "__main__":
    main()
