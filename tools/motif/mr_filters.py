"""MOTIF-RACK ES のフィルターの型ごとの周波数特性を、ノイズの波形で測る。
  python mr_filters.py --dir 置き場 [--types 0-21] [--rec ../../build/rec.exe]

雛形の音色（mr_ref.py）で波形 705（Noise）を鳴らし、フィルターを通さない状態（カットオフ 255・LPF24D）を基準に、
型 × カットオフ × レゾナンスごとの「基準との差（dB）」を、対数の刻みの周波数で CSV に出す（responses.csv）。
録音そのものも残す。もうある録音は飛ばす。送るのはパラメーターチェンジとノートだけ。"""
import csv
import os
import sys
import time

import numpy as np

import mr_ref

CUTOFFS = (64, 96, 128, 160, 192, 224)
RESOS = (0, 48, 96)
FREQS = [20 * 2 ** (i / 6) for i in range(61)]          # 20Hz〜20kHz を 1/6 オクターブ刻み
NOTE = 60
HOLD = 1.6


def spectrum(x):
    x = np.asarray(x)
    n = 8192
    acc = np.zeros(n // 2 + 1)
    k = 0
    for s in range(0, len(x) - n, n // 2):
        acc += np.abs(np.fft.rfft(x[s:s + n] * np.hanning(n))) ** 2
        k += 1
    return acc / max(k, 1)


def bands(p):
    f = np.fft.rfftfreq(8192, 1 / 44100)
    out = []
    for fc in FREQS:
        lo, hi = fc / 2 ** (1 / 12), fc * 2 ** (1 / 12)
        m = (f >= lo) & (f < hi)
        if not m.any():
            m = np.zeros(len(f), bool)
            m[int(np.argmin(np.abs(f - fc)))] = True
        out.append(float(p[m].mean()))
    return np.array(out)


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
    ts = opt(a, "--types", "0-21")
    lo, hi = (int(v) for v in ts.split("-")) if "-" in ts else (int(ts), int(ts))
    r = mr_ref.Ref(opt(a, "--midi", "MOTIF-RACK ES-1"), opt(a, "--rec", "../../build/rec.exe"), opt(a, "--dev", "0"), d)

    def take(name):
        path = os.path.join(d, name + ".wav")
        if os.path.exists(path):
            L, _ = mr_ref.read_wav(path)
        else:
            L, _ = r.one_note(name, note=NOTE, hold=HOLD, tail=0.3, pre=0.4)
        on = int(np.argmax(np.abs(np.asarray(L)) > 1e-4))
        return bands(spectrum(L[on + 4410:on + int((HOLD - 0.1) * 44100)]))

    rows = []
    try:
        r.all_off()
        r.mode(0)
        r.program(63, 0, 0)
        r.template(705)
        r.el(0, 0x43, 0x00, 0)
        r.el(0, 0x43, 0x01, 1, 127)
        r.el(0, 0x43, 0x05, 0)
        ref = take("ref_open")
        for t in range(lo, hi + 1):
            r.el(0, 0x43, 0x00, t)
            for co in CUTOFFS:
                r.el(0, 0x43, 0x01, co >> 7, co & 0x7F)
                for q in RESOS:
                    r.el(0, 0x43, 0x05, q)
                    time.sleep(0.05)
                    b = take("t%02d_c%03d_q%03d" % (t, co, q))
                    rows.append([t, co, q] + ["%.2f" % v for v in 10 * np.log10((b + 1e-30) / (ref + 1e-30))])
            print("type %d done" % t, flush=True)
    finally:
        r.close()
    with open(os.path.join(d, "responses.csv"), "w", newline="\n") as f:
        wr = csv.writer(f, lineterminator="\n")
        wr.writerow(["type", "cutoff", "resonance"] + ["%.1f" % v for v in FREQS])
        wr.writerows(rows)
    print("DONE %d rows" % len(rows))


if __name__ == "__main__":
    main()
