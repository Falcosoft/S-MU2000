"""MOTIF-RACK ES と SysEx をやりとりする道具（Windows の winmm を直接使う。追加のライブラリは要らない）。
送るのは Data List に載っているダンプ要求だけ。

  python mr_midi.py list
  python mr_midi.py monitor --in 名前 [--seconds 30] [--save 受けた.syx]
        受けた SysEx をそのまま落とす（パネルからのバルクダンプを受けるとき）
  python mr_midi.py dump --in 名前 --out 名前 --mm 00 --nn 0-127 --dir 置き場 [--dev 0]
        バルクのヘッダの番地 0E mm nn を 1 つずつ要求して、フッタ（0F mm nn）が来るまで受け、
        チェックサムを確かめて <置き場>/<mm>_<nn>.syx に置く。もうあるものは飛ばす

名前は部分一致。mm と nn は 16 進と 10 進が混ざると危ないので、mm は 16 進、nn は 10 進で受ける。"""
import ctypes
import ctypes.wintypes as wt
import os
import sys
import time

import mr_bulk

winmm = ctypes.WinDLL("winmm")
MHDR_DONE = 1
BUF = 8192
NBUF = 32


class MIDIHDR(ctypes.Structure):
    _fields_ = [("lpData", ctypes.c_void_p), ("dwBufferLength", wt.DWORD), ("dwBytesRecorded", wt.DWORD),
                ("dwUser", ctypes.c_size_t), ("dwFlags", wt.DWORD), ("lpNext", ctypes.c_void_p),
                ("reserved", ctypes.c_size_t), ("dwOffset", wt.DWORD), ("dwReserved", ctypes.c_size_t * 8)]


class MIDIINCAPS(ctypes.Structure):
    _fields_ = [("wMid", wt.WORD), ("wPid", wt.WORD), ("vDriverVersion", wt.UINT),
                ("szPname", ctypes.c_wchar * 32), ("dwSupport", wt.DWORD)]


class MIDIOUTCAPS(ctypes.Structure):
    _fields_ = [("wMid", wt.WORD), ("wPid", wt.WORD), ("vDriverVersion", wt.UINT),
                ("szPname", ctypes.c_wchar * 32), ("wTechnology", wt.WORD), ("wVoices", wt.WORD),
                ("wNotes", wt.WORD), ("wChannelMask", wt.WORD), ("dwSupport", wt.DWORD)]


for f in (winmm.midiInOpen, winmm.midiOutOpen):
    f.argtypes = [ctypes.POINTER(wt.HANDLE), wt.UINT, ctypes.c_size_t, ctypes.c_size_t, wt.DWORD]
for f in (winmm.midiInPrepareHeader, winmm.midiInAddBuffer, winmm.midiInUnprepareHeader,
          winmm.midiOutPrepareHeader, winmm.midiOutLongMsg, winmm.midiOutUnprepareHeader):
    f.argtypes = [wt.HANDLE, ctypes.POINTER(MIDIHDR), wt.UINT]
for f in (winmm.midiInStart, winmm.midiInStop, winmm.midiInReset, winmm.midiInClose,
          winmm.midiOutReset, winmm.midiOutClose):
    f.argtypes = [wt.HANDLE]
winmm.midiOutShortMsg.argtypes = [wt.HANDLE, wt.DWORD]
winmm.midiInGetDevCapsW.argtypes = [ctypes.c_size_t, ctypes.POINTER(MIDIINCAPS), wt.UINT]
winmm.midiOutGetDevCapsW.argtypes = [ctypes.c_size_t, ctypes.POINTER(MIDIOUTCAPS), wt.UINT]


def in_ports():
    out = []
    for i in range(winmm.midiInGetNumDevs()):
        c = MIDIINCAPS()
        winmm.midiInGetDevCapsW(i, ctypes.byref(c), ctypes.sizeof(c))
        out.append(c.szPname)
    return out


def out_ports():
    out = []
    for i in range(winmm.midiOutGetNumDevs()):
        c = MIDIOUTCAPS()
        winmm.midiOutGetDevCapsW(i, ctypes.byref(c), ctypes.sizeof(c))
        out.append(c.szPname)
    return out


def find(ports, name):
    hit = [i for i, p in enumerate(ports) if name.lower() in p.lower()]
    if not hit:
        raise SystemExit("口が見つからない: %s  候補: %s" % (name, ports))
    exact = [i for i in hit if ports[i].lower() == name.lower()]
    return (exact or hit)[0]


class MidiIn:
    """SysEx だけを受ける。呼び出し元が poll() を回す（コールバックは使わない）"""

    def __init__(self, name):
        self.h = wt.HANDLE()
        idx = find(in_ports(), name)
        r = winmm.midiInOpen(ctypes.byref(self.h), idx, 0, 0, 0)
        if r:
            raise SystemExit("MIDI 入力が開けない（%d）: %s" % (r, in_ports()[idx]))
        self.name = in_ports()[idx]
        self.bufs = []
        for _ in range(NBUF):
            b = ctypes.create_string_buffer(BUF)
            hdr = MIDIHDR()
            hdr.lpData = ctypes.cast(b, ctypes.c_void_p)
            hdr.dwBufferLength = BUF
            winmm.midiInPrepareHeader(self.h, ctypes.byref(hdr), ctypes.sizeof(hdr))
            winmm.midiInAddBuffer(self.h, ctypes.byref(hdr), ctypes.sizeof(hdr))
            self.bufs.append((b, hdr))
        self.next = 0
        self.acc = bytearray()
        winmm.midiInStart(self.h)

    def poll(self):
        """届いた分を足して、F0…F7 でそろったメッセージを返す。順番は届いた順"""
        out = []
        while True:
            b, hdr = self.bufs[self.next]
            if not hdr.dwFlags & MHDR_DONE:
                break
            self.acc += b.raw[:hdr.dwBytesRecorded]
            hdr.dwBytesRecorded = 0
            hdr.dwFlags &= ~MHDR_DONE
            winmm.midiInAddBuffer(self.h, ctypes.byref(hdr), ctypes.sizeof(hdr))
            self.next = (self.next + 1) % NBUF
        while True:
            s = self.acc.find(b"\xf0")
            if s < 0:
                self.acc.clear()
                break
            e = self.acc.find(b"\xf7", s)
            if e < 0:
                del self.acc[:s]
                break
            out.append(bytes(self.acc[s:e + 1]))
            del self.acc[:e + 1]
        return out

    def close(self):
        winmm.midiInStop(self.h)
        winmm.midiInReset(self.h)
        for _, hdr in self.bufs:
            winmm.midiInUnprepareHeader(self.h, ctypes.byref(hdr), ctypes.sizeof(hdr))
        winmm.midiInClose(self.h)


class MidiOut:
    def __init__(self, name):
        self.h = wt.HANDLE()
        idx = find(out_ports(), name)
        r = winmm.midiOutOpen(ctypes.byref(self.h), idx, 0, 0, 0)
        if r:
            raise SystemExit("MIDI 出力が開けない（%d）: %s" % (r, out_ports()[idx]))
        self.name = out_ports()[idx]

    def sysex(self, data):
        b = ctypes.create_string_buffer(bytes(data), len(data))
        hdr = MIDIHDR()
        hdr.lpData = ctypes.cast(b, ctypes.c_void_p)
        hdr.dwBufferLength = len(data)
        hdr.dwBytesRecorded = len(data)
        winmm.midiOutPrepareHeader(self.h, ctypes.byref(hdr), ctypes.sizeof(hdr))
        r = winmm.midiOutLongMsg(self.h, ctypes.byref(hdr), ctypes.sizeof(hdr))
        t0 = time.time()
        while not hdr.dwFlags & MHDR_DONE and time.time() - t0 < 2:
            time.sleep(0.001)
        winmm.midiOutUnprepareHeader(self.h, ctypes.byref(hdr), ctypes.sizeof(hdr))
        if r:
            raise SystemExit("送れない（%d）" % r)

    def short(self, status, d1=0, d2=0):
        winmm.midiOutShortMsg(self.h, status | d1 << 8 | d2 << 16)

    def close(self):
        winmm.midiOutClose(self.h)


def bulk_ok(m):
    body = m[5:-2]
    return (len(m) >= 12 and m[1] == 0x43 and m[2] & 0xF0 == 0 and m[3:5] == b"\x7f\x00"
            and (body[0] << 7 | body[1]) == len(body) - 5 and (sum(body) + m[-2]) & 0x7F == 0)


def request_block(mi, mo, dev, mm, nn, timeout=3.0):
    """0E mm nn を要求し、0F mm nn が来るまでのバルクを返す。だめなら None"""
    mi.poll()                                           # 前の残りを捨てる
    mo.sysex(mr_bulk.make_request(dev, (0x0E, mm, nn)))
    got = []
    t0 = time.time()
    while time.time() - t0 < timeout:
        for m in mi.poll():
            if len(m) > 9 and m[1] == 0x43 and m[3:5] == b"\x7f\x00":
                got.append(m)
                t0 = time.time()
                if m[7:10] == bytes([0x0F, mm, nn]):
                    if all(bulk_ok(x) for x in got) and got[0][7:10] == bytes([0x0E, mm, nn]):
                        return got
                    return None
        time.sleep(0.002)
    return None


def parse_range(s):
    out = []
    for part in s.split(","):
        if "-" in part:
            a, b = part.split("-")
            out += list(range(int(a), int(b) + 1))
        else:
            out.append(int(part))
    return out


def opt(a, name, default=None):
    if name in a:
        i = a.index(name)
        v = a[i + 1]
        del a[i:i + 2]
        return v
    return default


def main():
    a = sys.argv[1:]
    if not a or a[0] == "list":
        print("入力:")
        for i, p in enumerate(in_ports()):
            print("  %d: %s" % (i, p))
        print("出力:")
        for i, p in enumerate(out_ports()):
            print("  %d: %s" % (i, p))
        return
    mode = a.pop(0)
    if mode == "monitor":
        mi = MidiIn(opt(a, "--in"))
        secs = float(opt(a, "--seconds", "30"))
        save = opt(a, "--save")
        print("受信中: %s（%g 秒）" % (mi.name, secs))
        data = bytearray()
        n = 0
        t0 = time.time()
        while time.time() - t0 < secs:
            for m in mi.poll():
                n += 1
                data += m
                print("%4d: %d バイト  %s%s" % (n, len(m), m[:12].hex(" "), " …" if len(m) > 12 else ""))
            time.sleep(0.005)
        mi.close()
        if save:
            open(save, "wb").write(data)
        print("%d 個、%d バイト" % (n, len(data)))
        return
    if mode == "dump":
        mi = MidiIn(opt(a, "--in"))
        mo = MidiOut(opt(a, "--out"))
        dev = int(opt(a, "--dev", "0"))
        mm = int(opt(a, "--mm"), 16)
        nns = parse_range(opt(a, "--nn"))
        d = opt(a, "--dir")
        os.makedirs(d, exist_ok=True)
        ok = bad = skip = 0
        for nn in nns:
            path = os.path.join(d, "%02X_%03d.syx" % (mm, nn))
            if os.path.exists(path):
                skip += 1
                continue
            got = None
            for _ in range(3):
                got = request_block(mi, mo, dev, mm, nn)
                if got:
                    break
                time.sleep(0.3)
            if got:
                open(path, "wb").write(b"".join(got))
                ok += 1
            else:
                bad += 1
                print("取れない: 0E %02X %02X" % (mm, nn))
        mi.close()
        mo.close()
        print("mm=%02X: 取れた %d、取れない %d、もうあった %d" % (mm, ok, bad, skip))
        raise SystemExit(1 if bad else 0)
    raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
