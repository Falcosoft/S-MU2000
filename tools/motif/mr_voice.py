"""mr_midi.py dump で取ったノーマルボイスのバルク（.syx）を読んで、エレメントの表を CSV で出す。
  python mr_voice.py 置き場/voice [-o voices.csv] [--waves waves.csv]

読む欄は Data List の「MIDI PARAMETER CHANGE TABLE (NORMAL VOICE ELEMENT)」の並び。
  41 ee 00（29 バイト）: 00 エレメントのスイッチ、03-04 波形番号（上位 7bit・下位 7bit）、08 パン、
                         0C・0D 鍵の範囲、0E・0F ベロシティの範囲
  41 ee 20（30 バイト）: 00 エレメントの音量、04-07 AEG の時間（アタック・ディケイ 1・ディケイ 2・リリース）、
                         08-0B AEG の高さ（初め・アタック・ディケイ 1・ディケイ 2）
  40 70 00（82 バイト）: 頭の 10 文字が音色の名前
--drum を付けると、ドラムボイス（mm = 20・21・28）を鍵ごとの表にする。
  47 kk 00（19 バイト）: 00 スイッチ、01 波形の種類（0 = プリセットの波形、1 = ボイス）、06-07 波形番号、0C パン
  47 kk 20（12 バイト）: 00 音量。kk = 0-72 が C0-C6（MIDI の 24-96）
  46 70 00: 頭の 10 文字が名前
ファイルの名前は <mm>_<nn>.syx（mm = バルクのヘッダの番地の 2 バイトめ、nn = 音色の番号）。"""
import csv
import os
import sys

import mr_bulk

BANKS = {0x00: "PRE1", 0x01: "PRE2", 0x02: "PRE3", 0x03: "PRE4", 0x04: "PRE5", 0x05: "PRE6",
         0x06: "GM", 0x08: "USR1", 0x09: "USR2", 0x0A: "USR3", 0x0F: "EDIT"}


def blocks(path):
    out = {}
    for _, m, closed in mr_bulk.split_sysex(open(path, "rb").read()):
        if closed and len(m) > 11:
            b = m[5:-2]
            out[tuple(b[2:5])] = b[5:]
    return out


def voice(path):
    b = blocks(path)
    name = bytes(b.get((0x40, 0x70, 0x00), b"")[:10]).decode("ascii", "replace").rstrip()
    els = []
    for e in range(4):
        o = b.get((0x41, e, 0x00))
        a = b.get((0x41, e, 0x20))
        if o is None or a is None or len(o) < 0x10 or len(a) < 0x0C:
            continue
        els.append({"element": e + 1, "switch": o[0], "wave": o[3] << 7 | o[4], "pan": o[8],
                    "note_lo": o[0x0C], "note_hi": o[0x0D], "vel_lo": o[0x0E], "vel_hi": o[0x0F],
                    "level": a[0], "aeg_time": list(a[4:8]), "aeg_level": list(a[8:12])})
    return name, els


DRUM_BANKS = {0x20: "DR-PRE", 0x21: "DR-GM", 0x28: "DR-USR", 0x2F: "DR-EDIT"}


def drum_main(d, out, waves):
    wr = csv.writer(out, lineterminator="\n")
    wr.writerow(["bank", "number", "name", "note", "switch", "wave_type", "wave", "wave_name", "pan", "level"])
    n = 0
    for f in sorted(os.listdir(d)):
        if not f.endswith(".syx"):
            continue
        mm, nn = int(f[:2], 16), int(f[3:6])
        if mm not in DRUM_BANKS:
            continue
        b = blocks(os.path.join(d, f))
        name = bytes(b.get((0x46, 0x70, 0x00), b"")[:10]).decode("ascii", "replace").rstrip()
        n += 1
        for kk in range(73):
            o = b.get((0x47, kk, 0x00))
            a = b.get((0x47, kk, 0x20))
            if o is None or a is None:
                continue
            w = o[6] << 7 | o[7]
            wr.writerow([DRUM_BANKS[mm], nn + 1, name, 24 + kk, o[0], o[1], w,
                         waves.get(w, "") if o[1] == 0 else "", o[0x0C], a[0]])
    print("# %d ドラムボイス" % n, file=sys.stderr)


def main():
    a = sys.argv[1:]
    out = sys.stdout
    drum = "--drum" in a
    if drum:
        a.remove("--drum")
    waves = {}
    if "--waves" in a:
        i = a.index("--waves")
        for r in list(csv.reader(open(a[i + 1], encoding="utf-8")))[1:]:
            waves[int(r[0])] = r[3]
        del a[i:i + 2]
    if "-o" in a:
        i = a.index("-o")
        out = open(a[i + 1], "w", encoding="utf-8", newline="\n")
        del a[i:i + 2]
    d = a[0]
    if drum:
        drum_main(d, out, waves)
        return
    wr = csv.writer(out, lineterminator="\n")
    wr.writerow(["bank", "number", "name", "element", "switch", "wave", "wave_name", "pan", "note_lo", "note_hi",
                 "vel_lo", "vel_hi", "level", "aeg_attack", "aeg_decay1", "aeg_decay2", "aeg_release",
                 "aeg_init_level", "aeg_attack_level", "aeg_decay1_level", "aeg_decay2_level"])
    n = 0
    for f in sorted(os.listdir(d)):
        if not f.endswith(".syx"):
            continue
        mm, nn = int(f[:2], 16), int(f[3:6])
        if mm not in BANKS:
            continue
        name, els = voice(os.path.join(d, f))
        n += 1
        for e in els:
            wr.writerow([BANKS[mm], nn + 1, name, e["element"], e["switch"], e["wave"], waves.get(e["wave"], ""),
                         e["pan"], e["note_lo"], e["note_hi"], e["vel_lo"], e["vel_hi"], e["level"]]
                        + e["aeg_time"] + e["aeg_level"])
    print("# %d 音色" % n, file=sys.stderr)


if __name__ == "__main__":
    main()
