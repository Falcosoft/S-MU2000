"""mr_sweep.py で録った下録り（w0001.wav …）から、鍵ごとのサンプルの割り当てを見立てる。
  python mr_zones.py 置き場 [波形番号 …] [-o zones2.csv]

隣り合う鍵 n と n+1 を比べて、次のどれかに分ける。
  S  同じサンプルを半音ぶん速く読んでいる（片方を 2^(1/12) 倍に縮めると重なる）
  =  同じサンプルを同じ速さで読んでいる（そのまま重なる。音程が頭打ちになった所や、音程を追わない波形）
  |  別のサンプル（どちらでも重ならない）= 境目
  .  どちらかが無音
mr_sweep.py が録りながら出す見立て（S か、それ以外か）より細かい。高い鍵で音程が頭打ちになる所を、境目と取り違えない。"""
import csv
import os
import sys

import numpy as np

ON = 0.40
N = 6000


def read24(path):
    d = open(path, "rb").read()
    p = d.find(b"data")
    raw = np.frombuffer(d, dtype=np.uint8, offset=p + 8)
    n = len(raw) // 6
    a = raw[:n * 6].reshape(n, 2, 3).astype(np.int32)
    v = a[:, :, 0] | (a[:, :, 1] << 8) | (a[:, :, 2] << 16)
    v = np.where(v & 0x800000, v - 0x1000000, v)
    return v[:, 0] / 8388608.0, v[:, 1] / 8388608.0


def corr(a2, b, n):
    best = 0.0
    for sh in range(-4, 5):
        u = a2[8 + sh:n - 8 + sh]
        v = b[8:n - 8]
        den = float(np.sqrt(np.dot(u, u) * np.dot(v, v)))
        if den > 0:
            best = max(best, float(np.dot(u, v)) / den)
    return best


def analyse(base):
    L, R = read24(base + ".wav")
    x = L + R
    marks = [(int(a), float(b)) for a, b in (l.split() for l in open(base + ".txt"))]
    segs = {}
    for n, t in marks:
        s = int((t - 0.03) * 44100)
        seg = x[s:s + int((ON + 0.05) * 44100)]
        hot = np.abs(seg) > 2e-4
        if hot.any():
            on = int(np.argmax(hot))
            segs[n] = seg[on:on + 12000]
    out = []
    for n in range(0, 127):
        a, b = segs.get(n), segs.get(n + 1)
        if a is None or b is None or len(a) < 7000 or len(b) < N + 16:
            out.append(".")
            continue
        # 高い鍵ほどサンプルを速く読み進むので、比べる長さを短くする（長いままだと、ループの回り方の違いで重ならなくなる）
        m = int(max(800, min(N, N * 2.0 ** (-(n - 72) / 12.0))))
        cs = corr(np.interp(np.arange(m) * 2 ** (1 / 12), np.arange(len(a)), a), b, m)
        ce = corr(a[:m], b, m)
        if cs >= 0.9 and cs >= ce:
            out.append("S")
        elif ce >= 0.9:
            out.append("=")
        else:
            out.append("|")
    return "".join(out), sorted(segs)


def main():
    a = sys.argv[1:]
    out = None
    if "-o" in a:
        i = a.index("-o")
        out = a[i + 1]
        del a[i:i + 2]
    d = a[0]
    waves = [int(v) for v in a[1:]]
    if not waves:
        waves = sorted(int(f[1:5]) for f in os.listdir(d) if f.startswith("w") and f.endswith(".wav") and f[1:5].isdigit())
    rows = []
    for w in waves:
        s, keys = analyse(os.path.join(d, "w%04d" % w))
        bounds = [i + 1 for i, c in enumerate(s) if c == "|"]
        first = keys[0] if keys else -1
        clamp = s.find("=")
        rows.append([w, first, len(bounds) + 1, clamp + 1 if clamp >= 0 else "", " ".join(map(str, bounds)), s])
        if not out:
            print("%4d first key %3d zones %2d  %s" % (w, first, len(bounds) + 1, s))
    if out:
        with open(out, "w", newline="\n") as f:
            wr = csv.writer(f, lineterminator="\n")
            wr.writerow(["wave", "first_sounding_key", "zones", "first_equal_key", "boundaries", "pattern"])
            wr.writerows(rows)
        print("%d 波形" % len(rows))


if __name__ == "__main__":
    main()
