"""MOTIF-RACK ES のフィルターのカットオフの目盛りを細かく測る（レゾナンス 0、ノイズの波形 705、鍵 60）。
  python mr_cutoff.py --dir 置き場 [--types 0,4] [--step 4] [--lo 0 --hi 255] [--rec ../../build/rec.exe]

型ごとに、カットオフ 0〜255 を step 刻みで鳴らし、THRU（型 21）との差の周波数特性から
-3dB・-6dB・-12dB の周波数を出して cutoff.csv に書く。キーフォローとスケーリングは効かせない。
送るのはパラメーターチェンジとノートだけ。録音そのものも残す（もうあるものは飛ばす）。"""
import csv
import math
import os
import sys
import time

import numpy as np

import mr_ref

NOTE = 60
HOLD = 2.0
N = 16384


def spectrum(x):
    x = np.asarray(x, dtype=np.float64)
    acc = np.zeros(N // 2 + 1)
    k = 0
    for s in range(0, len(x) - N, N // 2):
        acc += np.abs(np.fft.rfft(x[s:s + N] * np.hanning(N))) ** 2
        k += 1
    return acc / max(k, 1)


def smooth(p, f):
    """1/24 オクターブの幅でならす"""
    out = np.empty_like(p)
    for i, fc in enumerate(f):
        lo, hi = fc / 2 ** (1 / 48), fc * 2 ** (1 / 48)
        a, b = np.searchsorted(f, lo), np.searchsorted(f, hi, side="right")
        out[i] = p[max(a, 0):max(b, a + 1)].mean()
    return out


def crossing(f, y, lvl):
    """低いほうから見て、初めて lvl を下回る（LPF）または上回る（HPF）点。対数で補う"""
    m = f > 20
    f, y = f[m], y[m]
    for i in range(len(y) - 1):
        if (y[i] - lvl) * (y[i + 1] - lvl) < 0:
            t = (lvl - y[i]) / (y[i + 1] - y[i])
            return math.exp(math.log(f[i]) + t * (math.log(f[i + 1]) - math.log(f[i])))
    return float("nan")


def opt(a, name, default=None):
    if name in a:
        i = a.index(name)
        v = a[i + 1]
        del a[i:i + 2]
        return v
    return default


def main():
    a = sys.argv[1:]
    d = opt(a, "--dir", ".")
    types = [int(t) for t in opt(a, "--types", "0,4").split(",")]
    step = int(opt(a, "--step", "4"))
    lo, hi = int(opt(a, "--lo", "0")), int(opt(a, "--hi", "255"))
    r = mr_ref.Ref(opt(a, "--midi", "MOTIF-RACK ES-1"), opt(a, "--rec", "../../build/rec.exe"), opt(a, "--dev", "0"), d)
    f = np.fft.rfftfreq(N, 1 / 44100)

    def take(name):
        path = os.path.join(d, name + ".wav")
        if os.path.exists(path):
            L, _ = mr_ref.read_wav(path)
        else:
            L, _ = r.one_note(name, note=NOTE, hold=HOLD, tail=0.3, pre=0.4)
        on = int(np.argmax(np.abs(np.asarray(L)) > 1e-4))
        return smooth(spectrum(L[on + 4410:on + int((HOLD - 0.1) * 44100)]), f)

    rows = []
    try:
        r.all_off()
        r.mode(0)
        r.program(63, 0, 0)
        r.template(705)
        r.el(0, 0x43, 0x05, 0)                      # レゾナンス 0
        r.el(0, 0x43, 0x07, 0, 0)                   # HPF のカットオフ 0
        r.el(0, 0x43, 0x22, 64)                     # カットオフのキーフォロー 0
        r.el(0, 0x43, 0x2F, 64)                     # HPF のキーフォロー 0
        for off in (0x27, 0x29, 0x2B, 0x2D):
            r.el(0, 0x43, off, 1, 0)                # カットオフのスケーリングのずらし 0（128）
        r.el(0, 0x43, 0x00, 21)                     # THRU を基準に
        r.el(0, 0x43, 0x01, 1, 127)
        time.sleep(0.05)
        ref = take("thru")
        for t in types:
            r.el(0, 0x43, 0x00, t)
            for co in range(lo, hi + 1, step):
                r.el(0, 0x43, 0x01, co >> 7, co & 0x7F)
                time.sleep(0.05)
                p = take("t%02d_c%03d" % (t, co))
                y = 10 * np.log10((p + 1e-30) / (ref + 1e-30))
                pas = float(np.median(y[(f > 25) & (f < 40)])) if t != 6 else float(np.median(y[(f > 15000) & (f < 18000)]))
                row = [t, co, round(pas, 2)] + [round(crossing(f, y, pas + lv), 1) for lv in (-3, -6, -12)]
                rows.append(row)
                print("type %d cutoff %3d: pass %+5.2f  -3dB %8.1f  -6dB %8.1f  -12dB %8.1f" % tuple(row), flush=True)
    finally:
        r.close()
    with open(os.path.join(d, "cutoff.csv"), "w", newline="\n") as fo:
        w = csv.writer(fo, lineterminator="\n")
        w.writerow(["type", "cutoff", "pass_db", "f_3db", "f_6db", "f_12db"])
        w.writerows(rows)
    print("DONE %d rows" % len(rows))


if __name__ == "__main__":
    main()
