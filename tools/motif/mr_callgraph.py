"""呼び出しグラフ。 cg.py up ADDR [depth] : ADDR を含む関数を呼ぶ側をさかのぼる
                  cg.py fn ADDR          : ADDR を含む関数の先頭と、そこから呼ぶ先
リテラルで関数の番地を持っているだけ（表・ポインタ渡し）のものも「参照」として数える。"""
import sys, struct, bisect, pickle, os
d = open("firm103.bin", "rb").read()
B = 0x8C020000
E = B + len(d)
N = len(d)


def w(a):
    return (d[a - B] << 8) | d[a - B + 1]


def l(a):
    return struct.unpack(">I", d[a - B:a - B + 4])[0]


def build():
    calls = []   # (site, target, kind)
    starts = set()
    # 関数の先頭の目安: STS.L PR,@-R15 (4f22) の手前に続く push 群の先頭
    for off in range(0, N - 1, 2):
        a = B + off
        op = w(a)
        if op == 0x4F22:
            s = a
            while s - 2 >= B and (w(s - 2) & 0xF0FF) == 0x2F06:
                s -= 2
            starts.add(s)
        if op >> 12 == 0xB:  # BSR
            disp = op & 0xFFF
            if disp & 0x800:
                disp -= 0x1000
            t = a + 4 + disp * 2
            if B <= t < E:
                calls.append((a, t, "bsr"))
        if op >> 12 == 0xD:  # MOV.L @(disp,PC),Rn
            p = (a & ~3) + 4 + (op & 0xFF) * 4
            if p + 4 <= E:
                v = l(p)
                if B <= v < E and v % 2 == 0:
                    calls.append((a, v, "lit"))
    # データの中の関数ポインタ（4 バイト境界の表）
    for off in range(0, N - 3, 4):
        v = struct.unpack(">I", d[off:off + 4])[0]
        if B <= v < E and v % 2 == 0:
            calls.append((B + off, v, "tbl"))
    for _, t, k in calls:
        if k == "bsr":
            starts.add(t)
    # lit/tbl の先は、先頭らしいものだけ関数とみなす
    return calls, sorted(starts)


if os.path.exists("cg.pkl"):
    calls, starts = pickle.load(open("cg.pkl", "rb"))
else:
    calls, starts = build()
    pickle.dump((calls, starts), open("cg.pkl", "wb"))
sset = set(starts)
by_t = {}
for s, t, k in calls:
    by_t.setdefault(t, []).append((s, k))


def fn(a):
    i = bisect.bisect_right(starts, a) - 1
    return starts[i] if i >= 0 else a


def up(a, depth, seen, ind=0):
    f = fn(a)
    if f in seen or ind > depth:
        return
    seen.add(f)
    for s, k in sorted(by_t.get(f, [])):
        print("%s%08x <- %08x (%s) in fn %08x" % ("  " * ind, f, s, k, fn(s)))
        if k != "tbl":
            up(s, depth, seen, ind + 1)


if sys.argv[1] == "up":
    up(int(sys.argv[2], 16), int(sys.argv[3]) if len(sys.argv) > 3 else 6, set())
elif sys.argv[1] == "fn":
    a = int(sys.argv[2], 16)
    f = fn(a)
    i = bisect.bisect_right(starts, a)
    nxt = starts[i] if i < len(starts) else E
    print("fn %08x .. %08x" % (f, nxt))
    for s, t, k in calls:
        if f <= s < nxt and k != "tbl" and t in sset:
            print("  %08x -> %08x (%s)" % (s, t, k))
