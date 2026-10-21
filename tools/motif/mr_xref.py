"""xref.py VALUE [VALUE...] : リテラルプールにその 32bit 値がある場所と、それを読む MOV.L の番地を出す"""
import sys, struct
d = open("firm103.bin", "rb").read()
BASE = 0x8C020000


def refs(val):
    pat = struct.pack(">I", val)
    out = []
    i = d.find(pat)
    while i >= 0:
        if i % 4 == 0:
            pool = BASE + i
            users = []
            # MOV.L @(disp,PC),Rn : Dnxx, target = (pc&~3)+4+disp*4
            for off in range(max(0, i - 1024), i, 2):
                w = (d[off] << 8) | d[off + 1]
                if w >> 12 == 0xD:
                    pc = BASE + off
                    if ((pc & ~3) + 4 + (w & 0xFF) * 4) == pool:
                        users.append(pc)
            out.append((pool, users))
        i = d.find(pat, i + 1)
    return out


for a in sys.argv[1:]:
    v = int(a, 16)
    for pool, users in refs(v):
        print("%08x: pool %08x used at %s" % (v, pool, " ".join("%08x" % u for u in users)))
