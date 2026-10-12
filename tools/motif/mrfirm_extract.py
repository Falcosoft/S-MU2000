#!/usr/bin/env python3
# license:BSD-3-Clause
"""MOTIF-RACK ES の更新データ（firm103.mid など）から、本体プログラムのイメージを取り出す。

使い方:
    python tools/motif/mrfirm_extract.py firm103.mid -o roms/motif-rack-es/firm103.bin

更新データは Standard MIDI File で、中身は SysEx だけ（doc/motif-rack-es.md）。
MU2000 の .ydl（tools/dump/ydl_extract.py）とほぼ同じ作りで、モデル ID が 6F になっている。

  F0 43 10 6F 00 7F 7F 7F 7F "MOTIF-RP" F7        開始
  F0 43 10 6F 02 a a a a  b b b b F7              消す範囲
  F0 43 10 6F 01 a a a a F7                       書き込み先
  F0 43 00 6F 02 x r r r <256B> ck F7             224 バイト。r は 21bit で一周する相対番地
  F0 43 00 6F 01 n r r r <nB>  ck F7              最後の 1 個（01 と n の 2 バイトで MIDI バイト数）
  F0 43 10 6F 03 a a a a s s s c c c c F7         区画の終わり。c = バイト総和の下位 28bit
  F0 43 10 6F 0E s s s s c c c F7                 全体の終わり。c = バイト総和の下位 16bit

7bit 詰めは「データ 7 バイト + 上位ビットを集めた 1 バイト（末尾、bit6 が先頭）」。
取り出したものは配らないこと（doc/dump/README.md の注意書きと同じ）。
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "dump"))
from ydl_extract import parse_smf, unpack7  # noqa: E402

MODEL = 0x6F


def val7(b: bytes) -> int:
    v = 0
    for x in b:
        v = (v << 7) | x
    return v


def extract(path: Path):
    """(イメージ, 記録されていた大きさ, 記録されていた和 28bit, 和 16bit, 化けたブロックの数)"""
    img = bytearray()
    size = sum28 = sum16 = None
    bad = 0
    for m in parse_smf(path):
        if len(m) < 5 or m[0] != 0x43 or m[2] != MODEL:
            continue
        cmd = m[3]
        if m[1] == 0x00 and cmd in (0x01, 0x02):
            # 番地は 21bit で一周するので使わず、届いた順に並べる
            img += unpack7(m[8:-2], False)
            bad += sum(m[3:-1]) % 128 != 0
        elif m[1] == 0x10 and cmd == 0x03:
            sum28 = val7(m[11:15])
        elif m[1] == 0x10 and cmd == 0x0E:
            size = val7(m[4:8])
            sum16 = val7(m[8:11])
    return img, size, sum28, sum16, bad


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mid", type=Path)
    ap.add_argument("-o", "--out", type=Path)
    a = ap.parse_args()

    img, size, sum28, sum16, bad = extract(a.mid)
    if size is None or sum28 is None:
        print("終わりのメッセージ（03 / 0E）が見つからない", file=sys.stderr)
        return 1
    # 最後のブロックは 7 バイト単位の端数ぶんだけ長い。記録の大きさで切る
    if len(img) < size:
        print(f"データが足りない: {len(img):#x} < {size:#x}", file=sys.stderr)
        return 1
    img = bytes(img[:size])
    s = sum(img)
    ok28 = (s & 0x0FFFFFFF) == sum28
    ok16 = (s & 0xFFFF) == sum16
    print(f"大きさ {size:#x}（{size:,} バイト）")
    print(f"版の文字列 {img[:16]!r}")
    print(f"ブロックのチェックサムが合わないもの: {bad}")
    print(f"全体の和 {s & 0x0FFFFFFF:#09x}  記録 {sum28:#09x} / {sum16:#06x}  -> {'一致' if ok28 and ok16 else '不一致'}")
    if a.out:
        a.out.parent.mkdir(parents=True, exist_ok=True)
        a.out.write_bytes(img)
        print(f"書き出し: {a.out}")
    return 0 if ok28 and ok16 and not bad else 1


if __name__ == "__main__":
    sys.exit(main())
