"""MOTIF-RACK ES の波形を 1 本ずつ、全部の鍵で短く鳴らして録る（鍵ごとのサンプルの境目を見つけるための下録り）。
  python mr_sweep.py --from 1 --to 1859 --dir 置き場 [--hours 8] [--rec ../../build/rec.exe] [--dev 0]

波形ごとに 1 つの WAV（24bit・ステレオ）と、鍵と時刻の表（.txt）を置く。もうあるものは飛ばすので、途中で止めて続きから回せる。
境目の見立ては zones.csv に 1 行ずつ足す（鍵 n と n+1 を、音程を合わせて比べた相関が低い所が境目）。
置き場に STOP という名前のファイルを置くと、今の波形を録り終えた所で止まる。

送るのはパラメーターチェンジとノートだけ（mr_ref.py の雛形の音色を使う）。ボイスモードに切り替えたままで終わる。"""
import os
import struct
import sys
import time

import mr_ref

KEY_LO, KEY_HI = 0, 127
ON, GAP = 0.40, 0.12


def write_wav24(path, L, R):
    n = len(L)
    try:
        import numpy as np
        arr = np.clip(np.rint(np.array([L, R], dtype=np.float64).T * 8388608.0), -8388608, 8388607).astype("<i4")
        arr = np.ascontiguousarray(arr)
        buf = arr.view(np.uint8).reshape(-1, 4)[:, :3].tobytes()
        n = -1
    except ImportError:
        buf = bytearray(n * 6)
    for i in range(max(n, 0)):
        for c, v in enumerate((L[i], R[i])):
            s = int(round(v * 8388608.0))
            s = max(-8388608, min(8388607, s))
            buf[i * 6 + c * 3:i * 6 + c * 3 + 3] = (s & 0xFFFFFF).to_bytes(3, "little")
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(buf)) + b"WAVEfmt " +
                struct.pack("<IHHIIHH", 16, 1, 2, 44100, 44100 * 6, 6, 24) + b"data" + struct.pack("<I", len(buf)))
        f.write(buf)


def zones(L, marks):
    """隣の鍵との相関。numpy があれば使う。無ければ空を返す"""
    try:
        import numpy as np
    except ImportError:
        return []
    x = np.array(L, dtype=np.float64)
    segs = {}
    for n, t in marks:
        s = int((t - 0.03) * 44100)
        seg = x[s:s + int((ON + 0.05) * 44100)]
        hot = np.abs(seg) > 1e-4
        if not hot.any():
            continue
        on = int(np.argmax(hot))
        segs[n] = seg[on:on + 12000]
    out = []
    N = 6000
    for n in sorted(segs):
        if n + 1 not in segs:
            continue
        a, b = segs[n], segs[n + 1]
        if len(a) < 7000 or len(b) < N + 16:
            continue
        a2 = np.interp(np.arange(N) * 2 ** (1 / 12), np.arange(len(a)), a)
        best = 0.0
        for sh in range(-3, 4):
            u = a2[8 + sh:N - 8 + sh]
            v = b[8:N - 8]
            den = float(np.sqrt(np.dot(u, u) * np.dot(v, v)))
            if den > 0:
                best = max(best, float(np.dot(u, v)) / den)
        out.append((n, best))
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
    d = opt(a, "--dir", ".")
    lo, hi = int(opt(a, "--from", "1")), int(opt(a, "--to", "1859"))
    hours = float(opt(a, "--hours", "8"))
    r = mr_ref.Ref(opt(a, "--midi", "MOTIF-RACK ES-1"), opt(a, "--rec", "../../build/rec.exe"), opt(a, "--dev", "0"), d)
    t_end = time.time() + hours * 3600
    done = 0
    try:
        r.all_off()
        r.mode(0)
        r.program(63, 0, 0)
        r.template(lo)
        for wave in range(lo, hi + 1):
            base = os.path.join(d, "w%04d" % wave)
            if os.path.exists(base + ".wav"):
                continue
            if time.time() > t_end or os.path.exists(os.path.join(d, "STOP")):
                break
            r.el(0, 0x41, 0x03, wave >> 7, wave & 0x7F)
            time.sleep(0.15)
            ev = []
            marks = []
            t = 0.4
            for n in range(KEY_LO, KEY_HI + 1):
                ev.append((t, lambda n=n: r.note_on(n, 100)))
                ev.append((t + ON, lambda n=n: r.note_off(n)))
                marks.append((n, t))
                t += ON + GAP
            L, R = r.record("_tmp", t + 0.4, ev)
            write_wav24(base + ".wav", L, R)
            os.remove(os.path.join(d, "_tmp.wav"))
            with open(base + ".txt", "w") as f:
                f.write("\n".join("%d %.3f" % m for m in marks))
            z = zones(L, marks)
            starts = [n + 1 for n, c in z if c < 0.9]
            pk_l, pk_r = mr_ref.peak(L), mr_ref.peak(R)
            with open(os.path.join(d, "zones.csv"), "a") as f:
                f.write("%d,%.5f,%.5f,%d,%s\n" % (wave, pk_l, pk_r, len(starts) + 1, " ".join(map(str, starts))))
            done += 1
            print("wave %d: peak %.3f/%.3f, %d zones" % (wave, pk_l, pk_r, len(starts) + 1), flush=True)
    finally:
        r.close()
    print("DONE %d waves" % done, flush=True)


if __name__ == "__main__":
    main()
