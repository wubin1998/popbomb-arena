#!/usr/bin/env python3
"""Standalone SWF container inspector / asset extractor (stdlib only).

Usage:
    python3 swf_inspect.py <input.swf> <output_dir>

Produces:
    <output_dir>/inventory.json
    <output_dir>/strings.txt
    <output_dir>/classes.txt
    <output_dir>/images/*.png|jpg
    <output_dir>/sounds/*.mp3|wav|bin
"""
import argparse
import json
import os
import struct
import sys
import zlib

TAG_NAMES = {
    0: "End", 1: "ShowFrame", 2: "DefineShape", 4: "PlaceObject",
    5: "RemoveObject", 6: "DefineBits", 7: "DefineButton", 8: "JPEGTables",
    9: "SetBackgroundColor", 10: "DefineFont", 11: "DefineText",
    12: "DoAction", 13: "DefineFontInfo", 14: "DefineSound", 15: "StartSound",
    17: "DefineButtonSound", 18: "SoundStreamHead", 19: "SoundStreamBlock",
    20: "DefineBitsLossless", 21: "DefineBitsJPEG2", 22: "DefineShape2",
    23: "DefineButtonCxform", 24: "Protect", 26: "PlaceObject2",
    28: "RemoveObject2", 32: "DefineShape3", 33: "DefineText2",
    34: "DefineButton2", 35: "DefineBitsJPEG3", 36: "DefineBitsLossless2",
    37: "DefineEditText", 39: "DefineSprite", 43: "FrameLabel",
    45: "SoundStreamHead2", 46: "DefineMorphShape", 48: "DefineFont2",
    56: "ExportAssets", 57: "ImportAssets", 58: "EnableDebugger",
    59: "DoInitAction", 60: "DefineVideoStream", 61: "VideoFrame",
    62: "DefineFontInfo2", 64: "EnableDebugger2", 65: "ScriptLimits",
    66: "SetTabIndex", 69: "FileAttributes", 70: "PlaceObject3",
    71: "ImportAssets2", 72: "DoABC", 73: "DefineFontAlignZones",
    74: "CSMTextSettings", 75: "DefineFont3", 76: "SymbolClass",
    77: "Metadata", 78: "DefineScalingGrid", 82: "DoABC2",
    83: "DefineShape4", 84: "DefineMorphShape2",
    86: "DefineSceneAndFrameLabelData", 87: "DefineBinaryData",
    88: "DefineFontName", 89: "StartSound2", 90: "DefineBitsJPEG4",
    91: "DefineFont4", 93: "EnableTelemetry", 94: "PlaceObject4",
}


def u8(b, o):
    return b[o]


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def read_rect(b, o):
    nbits = b[o] >> 3
    total = 5 + nbits * 4
    nbytes = (total + 7) // 8
    raw = int.from_bytes(b[o:o + nbytes], "big")
    shift = nbytes * 8 - 5
    vals = []
    # 5-bit nbits then 4 signed fields of nbits
    for _ in range(4):
        shift -= nbits
        v = (raw >> shift) & ((1 << nbits) - 1)
        if v & (1 << (nbits - 1)):
            v -= 1 << nbits
        vals.append(v)
    return vals, nbytes


def decompress_swf(path):
    data = open(path, "rb").read()
    sig = data[:3]
    version = data[3]
    file_len = u32(data, 4)
    if sig == b"CWS":
        body = zlib.decompress(data[8:])
    elif sig == b"ZWS":
        import lzma
        body = lzma.decompress(data[8:])
    elif sig == b"FWS":
        body = data[8:]
    else:
        raise SystemExit("Not an SWF: %r" % sig)
    return dict(sig=sig.decode(), version=version, file_len=file_len,
                compressed_len=len(data), body=body)


def png_chunk(kind, data):
    return (struct.pack(">I", len(data)) + kind + data +
            struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))


def write_png(path, w, h, rows, has_alpha, premultiplied=False):
    """rows: list of bytearray, each w*channels."""
    channels = 4 if has_alpha else 3
    out = bytearray()
    for row in rows:
        out.append(0)
        if premultiplied and has_alpha:
            fixed = bytearray(row)
            for i in range(0, len(fixed), 4):
                a = fixed[i]
                if a and a < 255:
                    fixed[i + 1] = min(255, fixed[i + 1] * 255 // a)
                    fixed[i + 2] = min(255, fixed[i + 2] * 255 // a)
                    fixed[i + 3] = min(255, fixed[i + 3] * 255 // a)
            out.extend(fixed)
        else:
            out.extend(row)
    color_type = 6 if has_alpha else 2
    ihdr = struct.pack(">IIBBBBB", w, h, 8, color_type, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(png_chunk(b"IHDR", ihdr))
        f.write(png_chunk(b"IDAT", zlib.compress(bytes(out), 9)))
        f.write(png_chunk(b"IEND", b""))


class Tagger:
    def __init__(self, body, outdir, extract):
        self.body = body
        self.outdir = outdir
        self.extract = extract
        self.tags = []
        self.counts = {}
        self.details = {
            "images": [], "sounds": [], "fonts": [], "frameLabels": [],
            "exportAssets": [], "symbolClasses": [], "editTexts": [],
            "sprites": [], "shapes": {"count": 0, "bytes": 0},
            "abc": [], "metadata": [], "background": None,
        }
        self.jpeg_tables = None
        self.abc_cache = []

    def parse(self):
        o = 0
        while o + 2 <= len(self.body):
            code_len = u16(self.body, o)
            code = code_len >> 6
            length = code_len & 0x3F
            o += 2
            if length == 0x3F:
                length = u32(self.body, o)
                o += 4
            data = self.body[o:o + length]
            self.record(code, data, o, 0)
            o += length
            if code == 0:
                break
        return o

    def record(self, code, data, offset, depth):
        name = TAG_NAMES.get(code, "Tag%d" % code)
        self.tags.append({"code": code, "name": name, "len": len(data),
                          "offset": offset, "depth": depth})
        self.counts[name] = self.counts.get(name, 0) + 1
        handler = getattr(self, "tag_%d" % code, None)
        if handler:
            handler(data, depth)

    # ---- character tags ----
    def tag_9(self, data, depth):
        if len(data) >= 3:
            self.details["background"] = "#%02x%02x%02x" % (data[0], data[1], data[2])

    def tag_43(self, data, depth):
        self.details["frameLabels"].append(data.rstrip(b"\x00").decode("utf-8", "replace"))

    def tag_77(self, data, depth):
        self.details["metadata"].append(data.rstrip(b"\x00").decode("utf-8", "replace"))

    def tag_56(self, data, depth):
        n = u16(data, 0)
        o = 2
        items = []
        for _ in range(n):
            cid = u16(data, o); o += 2
            end = data.index(b"\x00", o)
            items.append((cid, data[o:end].decode("utf-8", "replace")))
            o = end + 1
        self.details["exportAssets"] = items

    def tag_76(self, data, depth):
        n = u16(data, 0)
        o = 2
        items = []
        for _ in range(n):
            cid = u16(data, o); o += 2
            end = data.index(b"\x00", o)
            items.append((cid, data[o:end].decode("utf-8", "replace")))
            o = end + 1
        self.details["symbolClasses"].extend(items)

    def tag_75(self, data, depth):
        self._font(data, 75)

    def tag_48(self, data, depth):
        self._font(data, 48)

    def tag_91(self, data, depth):
        self._font(data, 91)

    def tag_88(self, data, depth):
        fid = u16(data, 0)
        o = 2
        end = data.index(b"\x00", o)
        name = data[o:end].decode("utf-8", "replace")
        o = end + 1
        end2 = data.index(b"\x00", o)
        copyright_ = data[o:end2].decode("utf-8", "replace")
        for f in self.details["fonts"]:
            if f["id"] == fid:
                f["name"] = name
                f["copyright"] = copyright_
                break
        else:
            self.details["fonts"].append({"id": fid, "name": name,
                                         "copyright": copyright_, "tag": 88})

    def _font(self, data, tag):
        fid = u16(data, 0)
        entry = {"id": fid, "tag": tag}
        # Font name sits behind flags whose width varies; take the first
        # printable null-terminated run near the tag header.
        for start in (3, 4, 5):
            end = data.find(b"\x00", start)
            if end > start:
                cand = data[start:end]
                if len(cand) >= 2 and all(32 <= c < 127 for c in cand):
                    entry["name"] = cand.decode("ascii")
                    break
        if tag == 91:
            entry["cff"] = True
        self.details["fonts"].append(entry)

    def tag_37(self, data, depth):
        cid = u16(data, 0)
        # Bounds rect then flags/strings; record raw printable strings
        try:
            _, rb = read_rect(data, 2)
            o = 2 + rb
            flags = u16(data, o); o += 2
            font_id = u16(data, o); o += 2
            if flags & 0x0001:  # HasFontClass
                o = data.index(b"\x00", o) + 1
            font_height = u16(data, o); o += 2
            if flags & 0x0002:  # HasFontData? actually hasFontClass above; HasMaxLength
                pass
            # skip: fontClass handled; HasMaxLength is flag 0x0002 -> u16
            if flags & 0x0002:
                o += 2
            # color (RGBA)
            if flags & 0x0004:
                o += 4
            if flags & 0x0008:  # HasMaxLength? spec: 0x0008 = HasLayout? we only need strings
                pass
            if flags & 0x0010:
                o += 2
            if flags & 0x0020:
                o = data.index(b"\x00", o) + 1
            if flags & 0x0040:
                o = data.index(b"\x00", o) + 1
            variable = ""
            if flags & 0x0080:
                e = data.index(b"\x00", o); variable = data[o:e].decode("utf-8", "replace"); o = e + 1
            initial = ""
            if flags & 0x0100:
                e = data.index(b"\x00", o); initial = data[o:e].decode("utf-8", "replace"); o = e + 1
            self.details["editTexts"].append({"id": cid, "variable": variable,
                                              "initialText": initial,
                                              "fontId": font_id})
        except Exception as exc:  # noqa: BLE001
            self.details["editTexts"].append({"id": cid, "error": str(exc)})

    def tag_20(self, data, depth):
        self._bitmap_lossless(data, alpha=False)

    def tag_36(self, data, depth):
        self._bitmap_lossless(data, alpha=True)

    def _bitmap_lossless(self, data, alpha):
        cid = u16(data, 0)
        fmt = data[2]
        w = u16(data, 3)
        h = u16(data, 5)
        o = 7
        entry = {"id": cid, "width": w, "height": h, "format": fmt,
                 "alpha": alpha, "tag": 36 if alpha else 20}
        try:
            if fmt == 3:
                table_size = data[o] + 1; o += 1
                comp = zlib.decompress(data[o:])
                entry["colors"] = table_size
                if alpha:
                    channels = 4
                    rows = []
                    for y in range(h):
                        row = bytearray()
                        base = table_size * 4 + y * ((w + 3) & ~3)
                        for x in range(w):
                            idx = comp[base + x]
                            p = idx * 4
                            row += comp[p:p + 3]
                            row.append(comp[p + 3])
                        rows.append(row)
                    write_png(os.path.join(self.outdir, "images", "%d.png" % cid),
                              w, h, rows, True, premultiplied=True)
                else:
                    rows = []
                    for y in range(h):
                        row = bytearray()
                        base = table_size * 3 + y * ((w + 3) & ~3)
                        for x in range(w):
                            idx = comp[base + x]
                            p = idx * 3
                            row += comp[p:p + 3]
                        rows.append(row)
                    write_png(os.path.join(self.outdir, "images", "%d.png" % cid), w, h, rows, False)
            elif fmt == 4:
                comp = zlib.decompress(data[o:])
                rows = []
                stride = (w * 2 + 3) & ~3
                for y in range(h):
                    row = bytearray()
                    for x in range(w):
                        p = y * stride + x * 2
                        px = comp[p] | (comp[p + 1] << 8)
                        r = ((px >> 10) & 0x1F) * 255 // 31
                        g = ((px >> 5) & 0x1F) * 255 // 31
                        bl = (px & 0x1F) * 255 // 31
                        row += bytes((r, g, bl))
                    rows.append(row)
                write_png(os.path.join(self.outdir, "images", "%d.png" % cid), w, h, rows, False)
            elif fmt == 5:
                comp = zlib.decompress(data[o:])
                rows = []
                for y in range(h):
                    row = bytearray(comp[y * w * 4:(y + 1) * w * 4])
                    if not alpha:
                        # XRGB -> RGB
                        rgb = bytearray()
                        for x in range(w):
                            rgb += row[x * 4 + 1:x * 4 + 4]
                        row = rgb
                    rows.append(row)
                write_png(os.path.join(self.outdir, "images", "%d.png" % cid),
                          w, h, rows, alpha, premultiplied=alpha)
            entry["extracted"] = True
        except Exception as exc:  # noqa: BLE001
            entry["error"] = str(exc)
        self.details["images"].append(entry)

    def tag_8(self, data, depth):
        self.jpeg_tables = data

    def tag_6(self, data, depth):
        self._jpeg(data, 6, alpha=False)

    def tag_21(self, data, depth):
        self._jpeg(data, 21, alpha=False)

    def tag_35(self, data, depth):
        self._jpeg(data, 35, alpha=True)

    def tag_90(self, data, depth):
        self._jpeg(data, 90, alpha=True)

    def _jpeg(self, data, tag, alpha):
        cid = u16(data, 0)
        entry = {"id": cid, "tag": tag, "alpha": alpha}
        if tag in (35, 90):
            alpha_off = u32(data, 2)
            hdr = 6 if tag == 35 else 8
            jpg = data[hdr:alpha_off]
            adata = data[alpha_off:]
        else:
            jpg = data[2:]
            adata = None
        if tag == 6 and self.jpeg_tables and not jpg.startswith(b"\xff\xd8"):
            jpg = self.jpeg_tables + jpg
        path = os.path.join(self.outdir, "images", "%d.jpg" % cid)
        with open(path, "wb") as f:
            f.write(jpg)
        entry["extracted"] = True
        entry["bytes"] = len(jpg)
        if jpg[:8] == b"\x89PNG\r\n\x1a\n":
            entry["actual"] = "png"
        elif jpg[:3] == b"GIF":
            entry["actual"] = "gif"
        else:
            entry["actual"] = "jpeg"
            # dimensions from SOF marker
            try:
                p = jpg.index(b"\xff\xc0")
            except ValueError:
                loc = -1
                for m in (b"\xff\xc2", b"\xff\xc1"):
                    try:
                        loc = jpg.index(m)
                        break
                    except ValueError:
                        continue
                p = loc if loc >= 0 else None
            if p is not None and p + 9 < len(jpg):
                entry["height"] = (jpg[p + 5] << 8) | jpg[p + 6]
                entry["width"] = (jpg[p + 7] << 8) | jpg[p + 8]
        if adata:
            try:
                a = zlib.decompress(adata)
                entry["alphaLength"] = len(a)
            except Exception:
                pass
        self.details["images"].append(entry)

    def tag_14(self, data, depth):
        cid = u16(data, 0)
        flags = data[2]
        fmt = flags >> 4
        rate = (flags >> 2) & 3
        size = (flags >> 1) & 1
        stype = flags & 1
        count = u32(data, 3)
        payload = data[7:]
        rates = [5512, 11025, 22050, 44100]
        ext = {0: "wav", 1: "adpcm", 2: "mp3", 3: "wav", 4: "nelly", 5: "nelly",
               6: "nelly", 11: "speex"}.get(fmt, "bin")
        entry = {"id": cid, "format": fmt, "rate": rates[rate], "bits": 16 if size else 8,
                 "channels": 2 if stype else 1, "samples": count, "bytes": len(payload)}
        try:
            if fmt == 2 and payload[:3] == b"ID3":
                with open(os.path.join(self.outdir, "sounds", "%d.%s" % (cid, ext)), "wb") as f:
                    f.write(payload)
            elif fmt == 2 and len(payload) > 2:
                with open(os.path.join(self.outdir, "sounds", "%d.%s" % (cid, ext)), "wb") as f:
                    f.write(payload[2:])  # skip MP3 seek offset
            elif fmt in (0, 3):
                # wrap PCM
                pcm = payload
                if fmt == 0:
                    pcm = bytearray()
                    for i in range(0, len(payload) - 1, 2):
                        pcm += payload[i:i + 2][::-1]
                ch = entry["channels"]
                bps = entry["bits"] // 8
                byte_rate = rates[rate] * ch * bps
                block = ch * bps
                hdr = b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVEfmt "
                hdr += struct.pack("<IHHIIHH", 16, 1, ch, rates[rate], byte_rate, block, entry["bits"])
                hdr += b"data" + struct.pack("<I", len(pcm))
                with open(os.path.join(self.outdir, "sounds", "%d.wav" % cid), "wb") as f:
                    f.write(hdr + bytes(pcm))
            else:
                with open(os.path.join(self.outdir, "sounds", "%d.%s" % (cid, ext)), "wb") as f:
                    f.write(payload)
            entry["extracted"] = True
        except Exception as exc:  # noqa: BLE001
            entry["error"] = str(exc)
        self.details["sounds"].append(entry)

    def tag_39(self, data, depth):
        cid = u16(data, 0)
        frames = u16(data, 2)
        self.details["sprites"].append({"id": cid, "frames": frames, "bytes": len(data)})
        # nested tags
        o = 4
        while o + 2 <= len(data):
            code_len = u16(data, o)
            code = code_len >> 6
            ln = code_len & 0x3F
            o += 2
            if ln == 0x3F:
                ln = u32(data, o); o += 4
            self.record(code, data[o:o + ln], -1, depth + 1)
            o += ln
            if code == 0:
                break

    def region(self, data, o, n):
        return data[o:o + n]

    def tag_72(self, data, depth):
        self._abc(data, "DoABC")

    def tag_82(self, data, depth):
        self._abc(data, "DoABC2")

    def _abc(self, data, kind):
        # DoABC/DoABC2 layout: Flags UI32, Name STRING, abcFile
        starts = []
        try:
            name_end = data.index(b"\x00", 4)
            starts.append(name_end + 1)
        except ValueError:
            pass
        starts.append(4)
        last = None
        for start in starts:
            try:
                abc = ABC(data[start:])
                self.abc_cache.append(abc)
                self.details["abc"].append({
                    "kind": kind,
                    "abcStart": start,
                    "abcVersion": "%d.%d" % (abc.major, abc.minor),
                    "classes": len(abc.classes),
                    "methods": len(abc.method_names),
                    "strings": len(abc.strings),
                    "scripts": len(abc.scripts),
                })
                return
            except Exception as exc:  # noqa: BLE001
                last = repr(exc)
        self.details["abc"].append({"kind": kind, "error": last})


class ABC:
    def __init__(self, data):
        self.d = data
        self.o = 0
        self.read_pools()
        self.read_methods()
        self.read_metadata()
        self.read_classes()
        self.read_method_bodies()

    def u8(self):
        v = self.d[self.o]; self.o += 1
        return v

    def u30(self):
        b = self.d[self.o]; self.o += 1
        if not (b & 0x80):
            return b
        v = b & 0x7F
        shift = 7
        while True:
            b = self.d[self.o]; self.o += 1
            v |= (b & 0x7F) << shift
            shift += 7
            if not (b & 0x80):
                break
        return v

    def s32(self):
        u = self.u30()
        if u & 1:
            u = ~(u >> 1) & 0xFFFFFFFF
        else:
            u = u >> 1
        if u & 0x80000000:
            u -= 1 << 32
        return u

    def read_pools(self):
        self.minor = u16(self.d, 0)
        self.major = u16(self.d, 2)
        if not (30 <= self.major <= 60):
            raise ValueError("implausible ABC version %d.%d" % (self.major, self.minor))
        self.o = 4
        ints = [0]
        for _ in range(self.u30() - 1):
            ints.append(self.s32())
        self.ints = ints
        uints = [0]
        for _ in range(self.u30() - 1):
            uints.append(self.u30())
        self.uints = uints
        doubles = [0.0]
        for _ in range(self.u30() - 1):
            doubles.append(struct.unpack_from("<d", self.d, self.o)[0]); self.o += 8
        self.doubles = doubles
        strings = [""]
        for _ in range(self.u30() - 1):
            n = self.u30()
            strings.append(self.d[self.o:self.o + n].decode("utf-8", "replace"))
            self.o += n
        self.strings = strings
        self.namespaces = [(0, 0)]
        for _ in range(self.u30() - 1):
            kind = self.u8()
            name = self.u30()
            self.namespaces.append((kind, name))
        self.ns_sets = [[]]
        for _ in range(self.u30() - 1):
            n = self.u30()
            self.ns_sets.append([self.u30() for _ in range(n)])
        self.multinames = [("", 0, 0)]
        for _ in range(self.u30() - 1):
            kind = self.u8()
            if kind in (0x07, 0x0D):
                self.multinames.append(("qname", self.u30(), self.u30()))
            elif kind in (0x0F, 0x10):
                self.multinames.append(("rtq", self.u30(), 0))
            elif kind in (0x11, 0x12):
                self.multinames.append(("rtql", 0, 0))
            elif kind in (0x09, 0x0E):
                self.multinames.append(("multi", self.u30(), self.u30()))
            elif kind in (0x1B, 0x1C):
                self.multinames.append(("multil", self.u30(), 0))
            elif kind == 0x1D:
                base = self.u30()
                n = self.u30()
                params = [self.u30() for _ in range(n)]
                self.multinames.append(("type", base, params))
            else:
                raise ValueError("bad multiname kind 0x%02x at %d" % (kind, self.o))

    def read_methods(self):
        self.method_names = [""]
        self.methods = [None]
        for _ in range(self.u30() - 1):
            pcount = self.u30()
            ret = self.u30()
            params = [self.u30() for _ in range(pcount)]
            name = self.u30()
            flags = self.u8()
            opts = []
            if flags & 0x08:
                for _ in range(self.u30()):
                    opts.append((self.u30(), self.u8()))
            pnames = []
            if flags & 0x80:
                pnames = [self.u30() for _ in range(pcount)]
            self.methods.append({"ret": ret, "params": params, "name": name,
                                 "flags": flags, "pnames": pnames})
            self.method_names.append(self.strings[name] if name < len(self.strings) else "")

    def read_metadata(self):
        self.metadata = [None]
        for _ in range(self.u30() - 1):
            name = self.u30()
            n = self.u30()
            items = [(self.u30(), self.u30()) for _ in range(n)]
            self.metadata.append({"name": self.strings[name], "items": items})

    def traits(self):
        out = []
        for _ in range(self.u30()):
            name_idx = self.u30()
            kind = self.u8()
            kind_low = kind & 0x0F
            info = {"name": self.strings[name_idx] if name_idx < len(self.strings) else "?",
                    "kind": kind_low}
            if kind_low in (0, 6):  # slot / const
                info["slot_id"] = self.u30()
                info["type"] = self.u30()
                vindex = self.u30()
                if vindex:
                    info["value_kind"] = self.u8()
                    vk = info["value_kind"]
                    if vk in (0x01, 0x02, 0x03):
                        info["value"] = self.u30()
                    elif vk == 0x04:
                        si = self.u30()
                        info["value"] = self.strings[si] if si < len(self.strings) else ""
                    elif vk in (0x05, 0x06):
                        self.u30()
                    elif vk in (0x08, 0x09, 0x0A, 0x0B):
                        pass
                    else:
                        raise ValueError("bad value kind %d" % vk)
            elif kind_low in (1, 2, 3, 5):  # method/getter/setter/function
                info["disp_id"] = self.u30()
                info["method"] = self.u30()
            elif kind_low == 4:  # class
                info["disp_id"] = self.u30()
                info["class"] = self.u30()
            else:
                raise ValueError("bad trait kind %d" % kind_low)
            if kind & 0x40:
                info["metadata"] = [self.u30() for _ in range(self.u30())]
            out.append(info)
        return out

    def read_classes(self):
        self.classes = []
        self.scripts = []
        n = self.u30()
        for _ in range(n):
            name_idx = self.u30()
            super_idx = self.u30()
            flags = self.u8()
            protected = self.u30() if flags & 0x08 else 0
            interfaces = [self.u30() for _ in range(self.u30())]
            iinit = self.u30()
            itraits = self.traits()
            cinit = self.u30()
            ctraits = self.traits()
            self.classes.append({
                "name_idx": name_idx, "super_idx": super_idx, "flags": flags,
                "protected": protected, "interfaces": interfaces,
                "iinit": iinit, "itraits": itraits,
                "cinit": cinit, "ctraits": ctraits,
            })
        for _ in range(self.u30()):
            init = self.u30()
            straits = self.traits()
            self.scripts.append({"init": init, "traits": straits})

    def read_method_bodies(self):
        self.bodies = [None] * len(self.methods)
        for _ in range(self.u30()):
            mid = self.u30()
            max_stack = self.u30()
            locals_ = self.u30()
            init_scope = self.u30()
            max_scope = self.u30()
            code_len = self.u30()
            self.o += code_len
            for _ in range(self.u30()):
                self.o += 5 * 0
                self.u30(); self.u30(); self.u30(); self.u30(); self.u30()
            self.traits()
            self.bodies[mid] = {"max_stack": max_stack, "locals": locals_,
                                "code_len": code_len}

    def qname(self, idx):
        if idx == 0 or idx >= len(self.multinames):
            return "*"
        kind, a, b = self.multinames[idx]
        if kind in ("qname", "multi"):
            ns = self.namespaces[a] if a < len(self.namespaces) else (0, 0)
            ns_name = self.strings[ns[1]] if ns[1] < len(self.strings) else ""
            name = self.strings[b] if b < len(self.strings) else ""
            return (ns_name + "::" + name) if ns_name else name
        if kind == "type":
            return self.qname(a) + "$" + "$".join(self.qname(p) for p in b)
        if kind == "multil":
            return "*"
        return "*"

    def class_name(self, cls):
        return self.qname(cls["name_idx"])

    def namespace_name(self, ns_idx):
        if ns_idx < len(self.namespaces):
            return self.strings[self.namespaces[ns_idx][1]]
        return ""


def export_abc_report(abc_list, outdir):
    classes = []
    all_strings = set()
    for abc in abc_list:
        all_strings.update(abc.strings)
        for cls in abc.classes:
            name = abc.class_name(cls)
            methods = []
            for t in cls["itraits"]:
                if t["kind"] in (1, 2, 3, 5):
                    methods.append({"name": t["name"],
                                    "method": abc.method_names[t["method"]] if t.get("method", 0) < len(abc.method_names) else "",
                                    "kind": t["kind"]})
            slots = [{"name": t["name"], "kind": t["kind"]} for t in cls["itraits"] if t["kind"] in (0, 6)]
            classes.append({
                "class": name,
                "super": abc.qname(cls["super_idx"]),
                "interfaces": [abc.qname(i) for i in cls["interfaces"]],
                "traits": slots,
                "methods": methods,
            })
        for script in abc.scripts:
            for t in script["traits"]:
                classes.append({"script": True, "trait": t["name"], "kind": t["kind"]})
    classes.sort(key=lambda c: c.get("class", ""))
    with open(os.path.join(outdir, "classes.txt"), "w") as f:
        for c in classes:
            if c.get("script"):
                f.write("[script] var %s\n" % c["trait"])
                continue
            f.write("class %s extends %s" % (c["class"], c["super"]))
            if c["interfaces"]:
                f.write(" implements " + ", ".join(c["interfaces"]))
            f.write("\n")
            for s in c["traits"]:
                f.write("    var %s\n" % s["name"])
            for m in c["methods"]:
                f.write("    function %s()\n" % m["name"])
    strings = sorted(s for s in all_strings if s)
    with open(os.path.join(outdir, "strings.txt"), "w") as f:
        f.write("\n".join(strings))
    return classes, strings


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("outdir")
    args = ap.parse_args()

    info = decompress_swf(args.input)
    body = info["body"]
    os.makedirs(os.path.join(args.outdir, "images"), exist_ok=True)
    os.makedirs(os.path.join(args.outdir, "sounds"), exist_ok=True)

    rect, rb = read_rect(body, 0)
    fps_raw = u16(body, rb)
    frame_count = u16(body, rb + 2)
    tag_start = rb + 4
    twips_w = rect[1] - rect[0]
    twips_h = rect[3] - rect[2]

    tagger = Tagger(body[tag_start:], args.outdir, True)
    end = tagger.parse()
    classes, strings = export_abc_report(tagger.abc_cache, args.outdir)

    inv = {
        "file": os.path.basename(args.input),
        "signature": info["sig"],
        "swfVersion": info["version"],
        "declaredFileLength": info["file_len"],
        "compressedBytes": info["compressed_len"],
        "decompressedBytes": len(body),
        "widthPx": twips_w / 20.0,
        "heightPx": twips_h / 20.0,
        "fps": fps_raw / 256.0,
        "frameCount": frame_count,
        "tagCount": len(tagger.tags),
        "tagHistogram": dict(sorted(tagger.counts.items(), key=lambda kv: -kv[1])),
        "details": tagger.details,
        "classCount": len([c for c in classes if not c.get("script")]),
        "stringCount": len(strings),
    }
    with open(os.path.join(args.outdir, "inventory.json"), "w") as f:
        json.dump(inv, f, indent=2, ensure_ascii=False)
    print(json.dumps({k: inv[k] for k in ("widthPx", "heightPx", "fps", "frameCount",
                                          "swfVersion", "tagCount", "classCount",
                                          "stringCount")}, indent=2))
    print("tag histogram:")
    for k, v in list(inv["tagHistogram"].items())[:25]:
        print("  %-28s %d" % (k, v))


if __name__ == "__main__":
    main()
