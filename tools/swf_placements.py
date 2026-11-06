#!/usr/bin/env python3
"""Print FrameLabels and PlaceObject2 matrices per SWF frame (read-only)."""
import struct
import sys
import zlib

def u8(b, o): return b[o]
def u16(b, o): return struct.unpack_from("<H", b, o)[0]
def u32(b, o): return struct.unpack_from("<I", b, o)[0]

def read_rect(b, o):
    nbits = b[o] >> 3
    total = 5 + nbits * 4
    nbytes = (total + 7) // 8
    raw = int.from_bytes(b[o:o + nbytes], "big")
    shift = nbytes * 8 - 5
    vals = []
    for _ in range(4):
        shift -= nbits
        v = (raw >> shift) & ((1 << nbits) - 1)
        if v & (1 << (nbits - 1)): v -= 1 << nbits
        vals.append(v)
    return vals, nbytes

class Bits:
    def __init__(self, b, byte=0):
        self.b = b
        self.pos = byte * 8

    def ub(self, n):
        v = 0
        for _ in range(n):
            byte = self.b[self.pos >> 3]
            bit = (byte >> (7 - (self.pos & 7))) & 1
            v = (v << 1) | bit
            self.pos += 1
        return v

    def sb(self, n):
        v = self.ub(n)
        if n and (v & (1 << (n - 1))):
            v -= 1 << n
        return v

    def align(self):
        self.pos = (self.pos + 7) & ~7


def read_matrix(b, o):
    br = Bits(b, o)
    sx = sy = 1.0
    r0 = r1 = 0.0
    if br.ub(1):
        n = br.ub(5)
        sx = br.sb(n) / 65536.0
        sy = br.sb(n) / 65536.0
    if br.ub(1):
        n = br.ub(5)
        r0 = br.sb(n) / 65536.0
        r1 = br.sb(n) / 65536.0
    n = br.ub(5)
    tx = br.sb(n) / 20.0
    ty = br.sb(n) / 20.0
    br.align()
    return (sx, sy, r0, r1, tx, ty), br.pos // 8

def cstr(b, o):
    e = b.index(b"\x00", o)
    return b[o:e].decode("utf-8", "replace"), e + 1

def parse_tags(b, o, end, depth, out):
    frame = 0
    root = out if isinstance(out, list) else out.setdefault("root", [])
    while o + 2 <= end:
        cl = u16(b, o); code = cl >> 6; ln = cl & 0x3F; o += 2
        if ln == 0x3F:
            ln = u32(b, o); o += 4
        data = b[o:o + ln]; o += ln
        if code == 0:
            break
        if code == 39:
            pass
        elif code == 26 or code == 70:
            flags = u8(data, 0); p = 1
            dep = u16(data, p); p += 2
            cid = None; mat = None; name = None
            if flags & 0x02:
                cid = u16(data, p); p += 2
            if flags & 0x04:
                mat, p = read_matrix(data, p)
            if flags & 0x08:
                br = Bits(data, p)
                has_add = br.ub(1)
                has_mult = br.ub(1)
                n = br.ub(4)
                if has_mult:
                    br.ub(n * 4)
                if has_add:
                    br.ub(n * 4)
                br.align()
                p = br.pos // 8
            if flags & 0x10: p += 2
            if flags & 0x20:
                name, p = cstr(data, p)
            if flags & 0x40: p += 2
            root.append(dict(frame=frame, depth=dep, cid=cid,
                                                   name=name, mat=mat, code=code))
        elif code == 43:
            label, _ = cstr(data, 0)
            root.append(dict(frame=frame, label=label))
        elif code == 1:
            frame += 1

def main(path):
    raw = open(path, "rb").read()
    if raw[:3] == b"CWS":
        body = zlib.decompress(raw[8:])
    elif raw[:3] == b"FWS":
        body = raw[8:]
    else:
        raise SystemExit("unsupported")
    _, rb = read_rect(body, 0)
    start = rb + 4  # frame rate + frame count
    out = {}
    parse_tags(body, start, len(body), 0, out)
    for item in out.get("root", []):
        if "label" in item:
            print("LABEL frame", item["frame"], item["label"])
    print("--- placements ---")
    for item in out.get("root", []):
        if "label" in item: continue
        mat = item["mat"]
        ms = "" if not mat else " scale=(%.3f,%.3f) rot=(%.3f,%.3f) pos=(%.1f,%.1f)" % (
            mat[0], mat[1], mat[2], mat[3], mat[4], mat[5])
        print("frame", item["frame"], "depth", item["depth"], "cid", item["cid"], "name", item["name"], ms)
    for sid, tags in out.items():
        if sid == "root": continue
        labels = [t for t in tags if "label" in t]
        if labels:
            print("SPRITE", sid, "labels", [(t["frame"], t["label"]) for t in labels])

if __name__ == "__main__":
    main(sys.argv[1])
