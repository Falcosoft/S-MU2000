"""関数ポインタを「オブジェクトの欄」に入れて呼ぶ作りを追う道具。
mr_callgraph.py はリテラルと相対分岐しか拾わないので、欄を経由する呼び出しで行き止まりになる。

  mr_slots.py fn ADDR      : その関数の番地を欄へ入れている所（どのレジスタ + 何バイトめか）
  mr_slots.py slot OFF     : 欄 OFF（16 進）へ関数を入れている所と、欄 OFF から取り出して呼んでいる所
  mr_slots.py calls LO HI  : 番地の範囲の中の、欄を経由する呼び出しを全部（どの欄をたどったか）
  mr_slots.py unref LO HI  : 範囲の中で、どこからも指されていない関数の先頭（リテラルにも相対分岐にも出てこない）

firm103.bin と cg.pkl のある場所で動かす。命令を上から順に数個さかのぼるだけなので、
分岐をまたぐ値や、計算で作る欄の番号は拾えない（拾えたものは確か、拾えないものがあっても無いとは言えない）。"""
import sys
import struct
import pickle
import bisect

d = open("firm103.bin", "rb").read()
B = 0x8C020000
E = B + len(d)


def w(a):
    return (d[a - B] << 8) | d[a - B + 1]


def l(a):
    return struct.unpack(">I", d[a - B:a - B + 4])[0]


def s8(x):
    return x - 256 if x & 0x80 else x


def imm_of(a, reg, depth=8):
    """a の手前で reg に入れられた定数（MOV #imm / MOV.W @(d,PC) / MOV.L @(d,PC)）。無ければ None"""
    for k in range(1, depth + 1):
        p = a - 2 * k
        if p < B + 0x40:
            break
        op = w(p)
        n = (op >> 8) & 15
        if n != reg:
            continue
        if op >> 12 == 0xE:
            return s8(op & 0xFF) & 0xFFFFFFFF
        if op >> 12 == 0x9:
            q = p + 4 + (op & 0xFF) * 2
            return w(q) if q + 2 <= E else None
        if op >> 12 == 0xD:
            q = ((p + 4) & ~3) + (op & 0xFF) * 4
            return l(q) if q + 4 <= E else None
        if op >> 12 == 0x7:      # ADD #imm,Rn の先までは追わない
            return None
    return None


def load_of(a, reg, depth=14):
    """a の手前で reg へ読み込んだ「欄」。(欄の番号, 元のレジスタ, その命令の番地) か None"""
    for k in range(1, depth + 1):
        p = a - 2 * k
        if p < B + 0x40:
            break
        op = w(p)
        n = (op >> 8) & 15
        m = (op >> 4) & 15
        if op >> 12 == 0x5 and n == reg:                 # MOV.L @(disp,Rm),Rn
            return (op & 15) * 4, m, p
        if op & 0xF00F == 0x000E and n == reg:           # MOV.L @(R0,Rm),Rn
            off = imm_of(p, 0)
            return (off if off is not None else -1), m, p
        if op & 0xF00F == 0x6002 and n == reg:           # MOV.L @Rm,Rn
            return 0, m, p
        if op >> 12 in (0x6, 0xE, 0xD, 0x9) and n == reg and op & 0xF00F != 0x6003:
            return None                                  # 別のもので上書きされた
        if op & 0xF00F == 0x6003 and n == reg:           # MOV Rm,Rn: 元をたどる
            reg = m
    return None


def stores():
    """関数の番地のリテラルを読んで、近くで欄へ書く所。(場所, 関数, 元のレジスタ, 欄の番号)"""
    out = []
    for a in range(B + 0x40, E - 1, 2):
        op = w(a)
        if op >> 12 != 0xD:
            continue
        q = ((a + 4) & ~3) + (op & 0xFF) * 4
        if q + 4 > E:
            continue
        v = l(q)
        if not (B + 0x40 <= v < E) or v & 1:
            continue
        reg = (op >> 8) & 15
        for k in range(1, 8):
            p = a + 2 * k
            if p + 2 > E:
                break
            o2 = w(p)
            n = (o2 >> 8) & 15
            m = (o2 >> 4) & 15
            if o2 >> 12 == 0x1 and m == reg:                 # MOV.L Rm,@(disp,Rn)
                out.append((p, v, n, (o2 & 15) * 4))
                break
            if o2 & 0xF00F == 0x0006 and m == reg:           # MOV.L Rm,@(R0,Rn)
                off = imm_of(p, 0)
                out.append((p, v, n, off if off is not None else -1))
                break
            if o2 & 0xF00F == 0x2002 and m == reg:           # MOV.L Rm,@Rn（番地はリテラルのことが多い）
                base = imm_of(p, n)
                out.append((p, v, n, ("abs", base) if base else 0))
                break
            if o2 >> 12 in (0xD, 0xE, 0x9, 0x6) and n == reg:
                break
            if o2 in (0x000B,) or o2 & 0xF0FF in (0x400B, 0x402B):
                break
    return out


def calls(lo=B + 0x40, hi=E):
    """JSR / JMP Rn のうち、Rn が欄から読まれたもの。(場所, [欄の番号…]) 欄は呼ぶ直前のものが最後"""
    out = []
    for a in range(lo, hi - 1, 2):
        op = w(a)
        if op & 0xF0FF not in (0x400B, 0x402B):
            continue
        reg = (op >> 8) & 15
        chain = []
        at = a
        for _ in range(3):
            r = load_of(at, reg)
            if r is None:
                break
            off, reg, at = r
            chain.append(off)
        if chain:
            out.append((a, list(reversed(chain))))
    return out


def fmt_off(o):
    if isinstance(o, tuple):
        return "[%08X]" % o[1]
    return "?" if o == -1 else "+%X" % o


def main():
    mode = sys.argv[1]
    if mode == "fn":
        t = int(sys.argv[2], 16)
        for p, v, n, off in stores():
            if v == t:
                print("%08x: %08x -> R%d %s" % (p, v, n, fmt_off(off)))
    elif mode == "slot":
        t = int(sys.argv[2], 16)
        print("-- 入れている所")
        for p, v, n, off in stores():
            if off == t:
                print("%08x: %08x -> R%d %s" % (p, v, n, fmt_off(off)))
        print("-- 取り出して呼んでいる所")
        for a, chain in calls():
            if chain[-1] == t:
                print("%08x: %s" % (a, " ".join(fmt_off(o) for o in chain)))
    elif mode == "calls":
        for a, chain in calls(int(sys.argv[2], 16), int(sys.argv[3], 16)):
            print("%08x: %s" % (a, " ".join(fmt_off(o) for o in chain)))
    elif mode == "unref":
        lo, hi = int(sys.argv[2], 16), int(sys.argv[3], 16)
        cg_calls, starts = pickle.load(open("cg.pkl", "rb"))
        # cg.pkl の「先頭」は STS.L PR の場所で、本当の先頭（レジスタを積み始める所）は少し手前にある。
        # 手前 0x20 バイトのどこかが指されていれば、指されていると数える
        targets = {t for _, t, _ in cg_calls}
        for a in range(B + 0x40, E - 1, 2):                  # 4 バイトの値として番地を持っている所も足す
            if a + 4 <= E:
                v = l(a)
                if lo - 0x20 <= v < hi:
                    targets.add(v)
        i = bisect.bisect_left(starts, lo)
        while i < len(starts) and starts[i] < hi:
            s = starts[i]
            if not any(t in targets for t in range(s - 0x20, s + 2, 2)):
                print("%08x" % s)
            i += 1
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
