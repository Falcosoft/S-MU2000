"""本体プログラムの中の波形の名前の表を CSV で出す。
  python mr_wavenames.py firm103.bin [-o waves.csv]
表は 12 バイトの行（分類 1 バイト + 名前 10 文字 + 0）で、0 番が Off、1 番からが波形。
分類の 2 文字の名前（"--"・"Ap"・"Ep"…）は、表の直前に 0 区切りで並んでいる。
表の場所は名前を目印に探す（版が変わっても動くように、番地は決め打ちしない）。"""
import sys

BASE = 0x8C020000


def find_table(d):
    o = d.find(b"Off       \x00") - 1
    if o < 0:
        raise SystemExit("波形の名前の表が見つからない")

    def ok(p):
        e = d[p:p + 12]
        return len(e) == 12 and e[11] == 0 and all(0x20 <= c < 0x7F for c in e[1:11])
    e = o
    while ok(e):
        e += 12
    return o, (e - o) // 12


def main():
    args = sys.argv[1:]
    out = sys.stdout
    if "-o" in args:
        i = args.index("-o")
        out = open(args[i + 1], "w", encoding="utf-8", newline="\n")
        del args[i:i + 2]
    d = open(args[0], "rb").read()
    o, n = find_table(d)
    s = d.rfind(b"--\x00", 0, o)
    cats = [c.decode("ascii") for c in d[s:o].split(b"\x00")]
    print("# table at %08X, %d rows" % (BASE + o, n), file=sys.stderr)
    out.write("number,category,category_name,name\n")
    for i in range(n):
        e = d[o + i * 12:o + i * 12 + 12]
        cat = cats[e[0]] if e[0] < len(cats) else "?"
        out.write("%d,%d,%s,%s\n" % (i, e[0], cat, e[1:11].decode("ascii").rstrip()))


if __name__ == "__main__":
    main()
