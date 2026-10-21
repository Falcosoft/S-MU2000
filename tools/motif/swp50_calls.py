"""SWP50 の読み書きの呼び出しを全部拾い、R4(チップ) R5(番地) R6(値) を定数伝播で追う。
  swpcalls.py            : 直接の呼び出し（関数ポインタ 8C33D664=書き / 8C33D668=読み 経由）
  swpcalls.py fn ADDR... : その関数を呼ぶ箇所の引数も出す（包みの関数用）
関数の中を上から順に 1 回なめるだけ（分岐は追わない）。値は式の文字列。"""
import sys, struct, pickle, bisect
d = open("firm103.bin", "rb").read()
B = 0x8C020000
E = B + len(d)
calls, starts = pickle.load(open("cg.pkl", "rb"))
WR, RD = 0x8C33D664, 0x8C33D668
PTRS = {WR: "W", RD: "R", 0x8C33D670: "X70", 0x8C33D674: "X74"}
DIRECT = {0x8C21CAD4: "W", 0x8C21CAE6: "R"}
extra = {}
if len(sys.argv) > 2 and sys.argv[1] == "fn":
    for a in sys.argv[2:]:
        extra[int(a, 16)] = "F%s" % a[-5:]


def w(a):
    return (d[a - B] << 8) | d[a - B + 1]


def l(a):
    return struct.unpack(">I", d[a - B:a - B + 4])[0]


def s8(x):
    return x - 256 if x & 0x80 else x


def s16(x):
    return x - 65536 if x & 0x8000 else x


# リテラルの置き場（データ）を先に印を付ける
data = set()
for off in range(0, len(d) - 1, 2):
    a = B + off
    op = w(a)
    if op >> 12 == 0xD:
        p = (a & ~3) + 4 + (op & 0xFF) * 4
        data.update((p, p + 2))
    elif op >> 12 == 0x9:
        p = a + 4 + (op & 0xFF) * 2
        data.add(p)


class V:
    __slots__ = ("c", "s")

    def __init__(self, c=None, s=None):
        self.c, self.s = c, s

    def __str__(self):
        if self.c is not None:
            return "%X" % (self.c & 0xFFFFFFFF)
        return self.s or "?"


def K(c):
    return V(c & 0xFFFFFFFF)


def S(s):
    return V(None, s if len(s) < 110 else "?")


U = V(None, "?")


def binop(a, b, sym, f):
    if a.c is not None and b.c is not None:
        return K(f(a.c, b.c))
    if str(a) == "?" and str(b) == "?":
        return U
    return S("(%s%s%s)" % (a, sym, b))


def unop(a, name, f):
    if a.c is not None:
        return K(f(a.c))
    if name in ("u16", "u8") and a.s and a.s != "?":
        return a  # 幅の切り詰めは式を汚すだけなので省く
    return S("%s(%s)" % (name, a)) if str(a) != "?" else U


out = []
mem = []


def run(f0, f1):
    r = [U] * 16
    for i in range(4):
        r[4 + i] = S("a%d" % i)
    a = f0
    pend = None  # (call 番地, kind) : 遅延スロットの後で確定
    while a < f1:
        if a in data:
            a += 2
            continue
        op = w(a)
        n, m, lo, hi = (op >> 8) & 15, (op >> 4) & 15, op & 15, op >> 12
        did_call = None
        if hi == 0xD:
            p = (a & ~3) + 4 + (op & 0xFF) * 4
            r[n] = K(l(p)) if p + 4 <= E else U
        elif hi == 0x9:
            p = a + 4 + (op & 0xFF) * 2
            r[n] = K(s16(w(p))) if p + 2 <= E else U
        elif hi == 0xE:
            r[n] = K(s8(op & 0xFF))
        elif hi == 0x7:
            r[n] = binop(r[n], K(s8(op & 0xFF)), "+", lambda x, y: x + y)
        elif hi == 0x6:
            if lo == 1:
                mem.append((a, f0, "r", str(r[m]), ""))
                r[n] = S("w[%s]" % r[m]) if str(r[m]) != "?" else U
            elif lo == 0:
                r[n] = S("b[%s]" % r[m]) if str(r[m]) != "?" else U
            elif lo == 3:
                r[n] = r[m]
            elif lo == 2:
                r[n] = S("[%s]" % r[m]) if str(r[m]) != "?" else U
            elif lo == 0xD:
                r[n] = unop(r[m], "u16", lambda x: x & 0xFFFF)
            elif lo == 0xC:
                r[n] = unop(r[m], "u8", lambda x: x & 0xFF)
            elif lo == 0xF:
                r[n] = unop(r[m], "s16", lambda x: s16(x & 0xFFFF))
            elif lo == 0xE:
                r[n] = unop(r[m], "s8", lambda x: s8(x & 0xFF))
            else:
                r[n] = U
                if lo in (4, 5, 6):
                    r[m] = U
        elif hi == 0x3:
            if lo == 0xC:
                r[n] = binop(r[n], r[m], "+", lambda x, y: x + y)
            elif lo == 0x8:
                r[n] = binop(r[n], r[m], "-", lambda x, y: x - y)
            elif lo in (0, 2, 3, 6, 7):
                pass
            else:
                r[n] = U
        elif hi == 0x2:
            if lo == 1:
                mem.append((a, f0, "w", str(r[n]), str(r[m])))
            elif lo == 0xB:
                r[n] = binop(r[n], r[m], "|", lambda x, y: x | y)
            elif lo == 0x9:
                r[n] = binop(r[n], r[m], "&", lambda x, y: x & y)
            elif lo == 0xA:
                r[n] = binop(r[n], r[m], "^", lambda x, y: x ^ y)
            elif lo in (4, 5, 6, 0xD, 0xE, 0xF):
                if lo >= 0xD:
                    r[n] = U
            # 0-2 は書き込み、7/8/C は比較
        elif hi == 0x4:
            b = op & 0xFF
            sh = {0x00: 1, 0x08: 2, 0x18: 8, 0x28: 16}
            shr = {0x01: 1, 0x09: 2, 0x19: 8, 0x29: 16}
            if b in sh:
                k = sh[b]
                r[n] = unop(r[n], "<<%d" % k, lambda x: x << k)
            elif b in shr:
                k = shr[b]
                r[n] = unop(r[n], ">>%d" % k, lambda x: (x & 0xFFFFFFFF) >> k)
            elif b == 0x21:
                r[n] = unop(r[n], ">>1", lambda x: (x & 0xFFFFFFFF) >> 1)
            elif b in (0x0B, 0x2B):
                did_call = r[n]
            elif b in (0x22, 0x15, 0x11, 0x2A, 0x0E, 0x1E, 0x2E, 0x0A, 0x1A):
                pass
            elif lo in (0xC, 0xD):
                r[n] = U
            else:
                r[n] = U
        elif hi == 0x0:
            if lo == 5:
                mem.append((a, f0, "w", str(binop(r[n], r[0], "+", lambda x, y: x + y)), str(r[m])))
            elif lo == 0xD:
                mem.append((a, f0, "r", str(binop(r[m], r[0], "+", lambda x, y: x + y)), ""))
            if lo in (0xC, 0xD, 0xE):
                ad = binop(r[m], r[0], "+", lambda x, y: x + y)
                r[n] = S("%s[%s]" % ("bwl"[lo - 0xC], ad)) if str(ad) != "?" else U
            elif op & 0xFF in (0x02, 0x0A, 0x1A, 0x2A, 0x29, 0x12, 0x22):
                r[n] = U
            elif lo == 7:
                pass
        elif hi == 0x5:
            r[n] = S("[%s+%X]" % (r[m], (op & 15) * 4)) if str(r[m]) != "?" else U
        elif hi == 0x8:
            if n == 1:
                mem.append((a, f0, "w", str(binop(r[m], K((op & 15) * 2), "+", lambda x, y: x + y)), str(r[0])))
            elif n == 5:
                mem.append((a, f0, "r", str(binop(r[m], K((op & 15) * 2), "+", lambda x, y: x + y)), ""))
            if n in (4, 5):
                r[0] = S("%s[%s+%X]" % ("bw"[n - 4], r[m], (op & 15) * (n - 3))) if str(r[m]) != "?" else U
        elif hi == 0xC:
            if n in (4, 5, 6, 7):
                r[0] = U
            elif n == 9:
                r[0] = binop(r[0], K(op & 0xFF), "&", lambda x, y: x & y)
            elif n == 0xB:
                r[0] = binop(r[0], K(op & 0xFF), "|", lambda x, y: x | y)
            elif n == 0xA:
                r[0] = U
        elif hi == 0xB:
            disp = op & 0xFFF
            if disp & 0x800:
                disp -= 0x1000
            did_call = K(a + 4 + disp * 2)
        elif hi == 0xF:
            pass
        # 呼び出しの確定（遅延スロットを実行した後の値で）
        if pend:
            site, tv = pend
            kind = None
            ts = str(tv)
            if tv.c is not None:
                kind = DIRECT.get(tv.c) or extra.get(tv.c)
            else:
                for p, kname in PTRS.items():
                    if ts == "[%X]" % p:
                        kind = kname
            if kind:
                out.append((site, f0, kind, str(r[4]), str(r[5]), str(r[6]), str(r[7])))
            for i in range(8):
                r[i] = U
            r[0] = S("ret")
            pend = None
        if did_call is not None:
            pend = (a, did_call)
        a += 2


if len(sys.argv) > 1 and sys.argv[1] == "vt":
    # vt TABLE N : メソッド表の各関数を先頭から次の関数までなめ、書き込みを出す
    extra[0x8C214336] = "Q"
    extra[0x8C2143A0] = "FLUSH"
    extra[0x8C214154] = "RD"
    tb, cnt = int(sys.argv[2], 16), int(sys.argv[3])
    ent = [l(tb + 4 * i) for i in range(cnt)]
    allf = sorted(set(ent))
    for i, f0 in enumerate(ent):
        j = allf.index(f0)
        f1 = allf[j + 1] if j + 1 < len(allf) else f0 + 0x200
        f1 = min(f1, f0 + 0x400)
        del out[:]
        del mem[:]
        run(f0, f1)
        print("== %d  %08x +%x" % (i, f0, f1 - f0))
        ev = [(x[0], "call %s r4=%s r5=%s r6=%s r7=%s" % x[2:]) for x in out]
        ev += [(x[0], "%s %s %s" % (x[2], x[3], ("<= " + x[4]) if x[2] == "w" else "")) for x in mem
               if "+C]" in x[3] or "<<8" in x[3]]
        for a, t in sorted(ev):
            print("   %08x %s" % (a, t[:150]))
    sys.exit()
for i, f0 in enumerate(starts):
    f1 = starts[i + 1] if i + 1 < len(starts) else E
    if f1 - f0 > 0x8000:
        f1 = f0 + 0x8000
    run(f0, f1)
if len(sys.argv) > 1 and sys.argv[1] == "mem":
    lo_, hi_ = int(sys.argv[2], 16), int(sys.argv[3], 16)
    for a, f0, k, ad, da in mem:
        if lo_ <= a < hi_:
            print("%08x fn %08x %s addr=%s data=%s" % (a, f0, k, ad, da))
    sys.exit()
for site, f0, kind, a4, a5, a6, a7 in out:
    print("%08x fn %08x %-6s chip=%s addr=%s data=%s r7=%s" % (site, f0, kind, a4, a5, a6, a7))
