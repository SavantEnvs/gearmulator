#!/usr/bin/env python3
"""Regenerates the starter seeds of the fuzz_lzo1x/smf/rcp/vstpreset/wav/sysex/midistream targets.

fuzz_frontpanel seeds come from gen_frontpanel.cpp (they use the library's own frame writers).
Run from the repo root: python3 mayhem/gen_seeds.py
"""
import os
import struct

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)))


def put(target, name, data, folder="testsuite"):
    d = os.path.join(ROOT, target, folder)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name), "wb") as f:
        f.write(bytes(data))


# ---------------------------------------------------------------- lzo1x
END = [0x11, 0x00, 0x00]


def seeded_run(zeros, last):
    return [0x14, ord('x'), ord('y'), ord('z'), 0x20] + [0] * zeros + [last, 0x08, 0x00]


def gen_lzo():
    t = "fuzz_lzo1x"
    put(t, "literal", [0x15] + list(b"abcd") + END)
    put(t, "m2", [0x14] + list(b"abc") + [0xa8, 0x00] + END)
    put(t, "m2_literal", [0x14] + list(b"abc") + [0xa9, 0x00, ord('Z')] + END)
    put(t, "m3", [0x14] + list(b"xyz") + [0x25, 0x08, 0x00] + END)
    put(t, "m3_long", seeded_run(1, 5) + END)
    put(t, "literal_run", [0x14] + list(b"abc") + [0xa8, 0x00, 0x02] + list(b"12345") + END)
    put(t, "m1", seeded_run(8, 24) + [0x01] + list(b"ABCD") + [0x00, 0x00] + END)
    put(t, "m4", seeded_run(66, 134) + [0x11, 0x04, 0x00] + END)
    put(t, "truncated", [0x15] + list(b"ab"))
    put(t, "bad_distance", [0x14] + list(b"abc") + [0xa8, 0x01] + END)


# ---------------------------------------------------------------- SMF
def vlq(v):
    out = [v & 0x7f]
    v >>= 7
    while v:
        out.append((v & 0x7f) | 0x80)
        v >>= 7
    return list(reversed(out))


def mtrk(events):
    body = bytes(events)
    return b"MTrk" + struct.pack(">I", len(body)) + body


def smf(fmt, division, tracks):
    return b"MThd" + struct.pack(">IHHH", 6, fmt, len(tracks), division) + b"".join(tracks)


def gen_smf():
    t = "fuzz_smf"
    tempo = [0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20]
    eot = [0x00, 0xff, 0x2f, 0x00]
    note = [0x00, 0x90, 60, 100] + vlq(480) + [0x80, 60, 0]
    put(t, "format0_note", smf(0, 480, [mtrk(tempo + note + eot)]))
    running = [0x00, 0x90, 60, 100, 0x10, 62, 100, 0x10, 64, 100] + vlq(240) + [0x80, 60, 0, 0x00, 62, 0, 0x00, 64, 0]
    put(t, "running_status", smf(0, 96, [mtrk(tempo + running + eot)]))
    sysex = [0x00, 0xf0, 0x07, 0x7e, 0x7f, 0x09, 0x01, 0xf7, 0x10, 0xf7, 0x03, 0x01, 0x02, 0x03]
    put(t, "sysex", smf(0, 480, [mtrk(sysex + note + eot)]))
    meta = [0x00, 0xff, 0x03, 0x04] + list(b"Trk1") + [0x00, 0xff, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08]
    ctrl = [0x00, 0xb0, 7, 100, 0x00, 0xc0, 5, 0x00, 0xe0, 0x00, 0x40, 0x00, 0xd0, 0x20, 0x00, 0xa0, 60, 30]
    put(t, "format1_two_tracks", smf(1, 480, [mtrk(tempo + meta + eot), mtrk(ctrl + note + eot)]))
    put(t, "smpte_division", smf(0, 0xe728, [mtrk(note + eot)]))
    put(t, "long_delta", smf(0, 480, [mtrk([0x00, 0x90, 60, 100] + vlq(0x0fffffff) + [0x80, 60, 0] + eot)]))
    put(t, "truncated", smf(0, 480, [mtrk(note)])[:-3])
    put(t, "format2", smf(2, 480, [mtrk(note + eot), mtrk(note + eot)]))


# ---------------------------------------------------------------- RCP
def rcp_v2(tracks, tempo=120, timebase=48, num_tracks=0):
    h = bytearray(0x586)
    h[0:28] = b"RCM-PC98V2.0(C)COME ON MUSIC"
    h[0x1c0] = timebase & 0xff
    h[0x1e7] = (timebase >> 8) & 0xff
    h[0x1c1] = tempo
    h[0x1c2] = 4
    h[0x1c3] = 4
    h[0x1e6] = num_tracks
    out = bytes(h)
    for channel, events in tracks:
        body = b"".join(bytes(e) for e in events)
        length = 0x2c + len(body)
        hdr = bytearray(0x2c)
        struct.pack_into("<H", hdr, 0, length)
        hdr[4] = channel
        hdr[5] = 0
        hdr[6] = 0
        out += bytes(hdr) + body
    return out


def rcp_g36(tracks, tempo=120, timebase=48):
    h = bytearray(0xc98)
    h[0:31] = b"COME ON MUSIC RECOMPOSER RCP3.0"
    struct.pack_into("<H", h, 0x208, len(tracks))
    struct.pack_into("<H", h, 0x20a, timebase)
    struct.pack_into("<H", h, 0x20c, tempo)
    h[0x20e] = 4
    h[0x20f] = 4
    out = bytes(h)
    for channel, events in tracks:
        body = b"".join(bytes(e) for e in events)
        length = 0x2e + len(body)
        hdr = bytearray(0x2e)
        struct.pack_into("<I", hdr, 0, length)
        hdr[6] = channel
        out += bytes(hdr) + body
    return out


def gen_rcp():
    t = "fuzz_rcp"
    ev = lambda c, a, b, d: [c, a, b, d]
    notes = [ev(60, 48, 40, 100), ev(64, 48, 40, 100), ev(67, 48, 40, 100), ev(0xe2, 0, 0, 0), ev(0xfe, 0, 0, 0)]
    put(t, "v2_one_track", rcp_v2([(0, notes)]))
    put(t, "v2_two_tracks", rcp_v2([(0, notes), (1, [ev(0xc0, 5, 0, 0), ev(72, 96, 90, 100), ev(0xfe, 0, 0, 0)])], num_tracks=2))
    put(t, "v2_repeat_loop", rcp_v2([(0, [ev(60, 48, 40, 100), ev(0xf9, 0, 0, 0), ev(62, 48, 40, 100), ev(0xf8, 0, 0, 0), ev(0xfe, 0, 0, 0)])]))
    put(t, "v2_header_only", rcp_v2([]))
    put(t, "g36_one_track", rcp_g36([(0, [bytes([60, 48, 0, 0, 0, 0]), bytes([0xfe, 0, 0, 0, 0, 0])])]))
    put(t, "g36_truncated", rcp_g36([(0, [bytes([60, 48, 0, 0, 0, 0])])])[:-2])


# ---------------------------------------------------------------- VST presets
def fxp(kind, params=3, chunk=b"", name=b"Init"):
    name = name.ljust(28, b"\0")[:28]
    body = b"FPCh" if kind == "opaque" else b"FxCk"
    body += struct.pack(">I", 1) + b"GEAR" + struct.pack(">II", 1, params) + name
    if kind == "opaque":
        body += struct.pack(">I", len(chunk)) + chunk
    else:
        body += b"".join(struct.pack(">f", 0.25 * i) for i in range(params))
    return b"CcnK" + struct.pack(">I", len(body)) + body


def fxb(kind, programs, version=2):
    body = b"FBCh" if kind == "opaque" else b"FxBk"
    body += struct.pack(">I", version) + b"GEAR" + struct.pack(">II", 1, len(programs))
    if version >= 2:
        body += struct.pack(">I", 0) + b"\0" * 124
    else:
        body += b"\0" * 128
    for p in programs:
        body += struct.pack(">I", len(p)) + p if kind == "opaque" else p
    return b"CcnK" + struct.pack(">I", len(body)) + body


def vst3(chunks):
    """chunks: list of (id4, data); the data area follows the 48 byte header, then the List."""
    head = b"VST3" + struct.pack("<I", 1) + b"0123456789ABCDEF0123456789ABCDEF"
    data = b""
    entries = []
    off = len(head) + 8
    for cid, d in chunks:
        entries.append((cid, off + len(data), len(d)))
        data += d
    list_off = len(head) + 8 + len(data)
    out = head + struct.pack("<Q", list_off) + data + b"List" + struct.pack("<I", len(entries))
    for cid, o, l in entries:
        out += cid + struct.pack("<QQ", o, l)
    return out


def gen_vst():
    t = "fuzz_vstpreset"
    payload = bytes(range(64))
    put(t, "fxp_opaque", fxp("opaque", chunk=payload))
    put(t, "fxp_regular", fxp("regular", params=5))
    put(t, "fxb_opaque", fxb("opaque", [payload, payload[:16], b""]))
    put(t, "fxb_regular", fxb("regular", [fxp("regular", 2), fxp("regular", 4)], version=1))
    put(t, "fxb_of_fxp_opaque", fxb("regular", [fxp("opaque", chunk=payload), fxp("opaque", chunk=b"abc")]))
    put(t, "vst3_comp", vst3([(b"Comp", payload), (b"Cont", b"controller state")]))
    put(t, "vst3_embedded_fxp", vst3([(b"Comp", fxp("opaque", chunk=payload))]))
    wrapped = b"VstW" + struct.pack(">III", 8, 1, 0) + fxp("opaque", chunk=payload)
    put(t, "vst3_vstw", vst3([(b"Comp", wrapped)]))
    put(t, "fxp_truncated", fxp("opaque", chunk=payload)[:60])


# ---------------------------------------------------------------- WAV
def wav(fmt_tag, channels, rate, bits, frames, extra=b""):
    block = channels * bits // 8
    data = bytes((i * 7) & 0xff for i in range(frames * block))
    fmt = struct.pack("<HHIIHH", fmt_tag, channels, rate, rate * block, block, bits)
    body = b"WAVE" + b"fmt " + struct.pack("<I", 16) + fmt + extra + b"data" + struct.pack("<I", len(data)) + data
    if len(data) & 1:
        body += b"\0"
    return b"RIFF" + struct.pack("<I", len(body)) + body


def cue_chunks(points, labels, align=4):
    cue = struct.pack("<I", len(points))
    for i, pos in enumerate(points):
        cue += struct.pack("<II4sIII", i + 1, pos, b"data", 0, 0, pos)
    out = b"cue " + struct.pack("<I", len(cue)) + cue
    lst = b"adtl"
    for i, name in enumerate(labels):
        txt = name.encode() + b"\0"
        while align == 4 and len(txt) & 3:
            txt += b"\0"
        lbl = struct.pack("<I", i + 1) + txt
        lst += b"labl" + struct.pack("<I", len(lbl)) + lbl + (b"\0" if len(lbl) & 1 else b"")
    out += b"LIST" + struct.pack("<I", len(lst)) + lst
    return out


def gen_wav():
    t = "fuzz_wav"
    put(t, "pcm16_mono", wav(1, 1, 44100, 16, 16))
    put(t, "pcm24_stereo", wav(1, 2, 48000, 24, 8))
    put(t, "float32_cues", wav(3, 1, 96000, 32, 16, cue_chunks([0, 8], ["start", "mid"])))
    put(t, "pcm8_cues", wav(1, 1, 22050, 8, 32, cue_chunks([4], ["loop"])))
    put(t, "odd-label-misaligned", wav(1, 1, 22050, 8, 32, cue_chunks([4], ["loop"], align=2)), "known-findings")
    put(t, "adpcm_rejected", wav(2, 1, 8000, 4, 4))


# ---------------------------------------------------------------- sysex
def gen_sysex():
    t = "fuzz_sysex"
    a = [0xf0, 0x43, 0x10, 0x4c, 0x00, 0x00, 0x7e, 0x00, 0xf7]
    b = [0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7]
    put(t, "single", a)
    put(t, "two", a + b)
    put(t, "double_framing", [0xf0, 0xf0, 0x43, 0x10, 0x4c, 0x00, 0x00, 0x7e, 0x00, 0xf7, 0xf7] + b)
    put(t, "no_end", a[:-1])
    put(t, "garbage_between", [0x00, 0x01] + a + [0x90, 0x3c, 0x40] + b + [0x55])
    note = [0x00, 0x90, 60, 100, 0x60, 0x80, 60, 0, 0x00, 0xff, 0x2f, 0x00]
    sx = [0x00, 0xf0, len(a) - 1] + a[1:]
    midi = smf(0, 480, [mtrk(sx + note)])
    put(t, "smf_with_sysex", midi)
    put(t, "tuning_dump", [0xf0, 0x7e, 0x7f, 0x08, 0x01, 0x00] + list(b"Tuning Name 16 b") + [0] * 20 + [0xf7])


# ---------------------------------------------------------------- MIDI byte stream
def gen_midistream():
    t = "fuzz_midistream"
    put(t, "notes_running_status", [0x90, 60, 100, 62, 100, 64, 100, 0x80, 60, 0, 62, 0, 64, 0])
    put(t, "channel_messages", [0xb0, 7, 100, 0xc1, 5, 0xe2, 0, 0x40, 0xd3, 0x20, 0xa4, 60, 30])
    put(t, "sysex_with_realtime", [0xf0, 0x43, 0x10, 0xf8, 0x4c, 0x00, 0xfe, 0x00, 0x7e, 0xf7, 0xfa, 0xfc])
    put(t, "system_common", [0xf1, 0x20, 0xf2, 0x10, 0x20, 0xf3, 0x05, 0xf6])
    put(t, "translate_skip", [0xf0, 0x60, 0x01, 0x90, 60, 100, 0xf7, 0x90, 60, 100])
    put(t, "translate_channels", [0xf0, 0x60, 0x03, 0xf7, 0xf0, 0x60, 0x02, 0x00, 0x05, 0xf7, 0x90, 60, 100, 0x80, 60, 0])
    put(t, "translate_reset", [0xf0, 0x60, 0x04, 0xf7, 0xf0, 0x60, 0x03, 0xf7, 0x91, 60, 100])
    put(t, "unterminated_sysex", [0xf0, 0x7e, 0x7f, 0x09, 0x01, 0x90, 60, 100])


if __name__ == "__main__":
    gen_lzo()
    gen_smf()
    gen_rcp()
    gen_vst()
    gen_wav()
    gen_sysex()
    gen_midistream()
