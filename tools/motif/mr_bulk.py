"""MOTIF-RACK ES のバルクダンプ（.syx）を確かめる道具。実機には何も送らない。
  python mr_bulk.py check dump.syx            : 中のメッセージを並べ、チェックサムを確かめる
  python mr_bulk.py request AH AM AL [--dev N]: ダンプ要求のバイト列を出す（16 進。番地は Data List から）
  python mr_bulk.py selftest                  : この道具の自己点検

形はサービスマニュアルの「MIDI DATA FORMAT (3-6-4)(3-6-5)」から。
  バルクダンプ: F0 43 0n 7F 00 bh bl ah am al <データ…> cs F7
  ダンプ要求  : F0 43 2n 7F 00 ah am al F7
チェックサムは「バイト数（bh bl）・番地・データ・チェックサム自身を足すと下位 7bit が 0」。
番地とバイト数の表は Data List（別冊）にあり、ここには持っていない。"""
import sys


def split_sysex(data):
    out = []
    i = 0
    while True:
        s = data.find(b"\xf0", i)
        if s < 0:
            break
        e = data.find(b"\xf7", s)
        if e < 0:
            out.append((s, data[s:], False))
            break
        out.append((s, data[s:e + 1], True))
        i = e + 1
    return out


def checksum(body):
    """body = バイト数 2 + 番地 3 + データ。返すのは付けるべきチェックサム"""
    return (-sum(body)) & 0x7F


def make_bulk(dev, addr, payload):
    body = bytes([(len(payload) >> 7) & 0x7F, len(payload) & 0x7F]) + bytes(addr) + bytes(payload)
    return bytes([0xF0, 0x43, dev & 0x0F, 0x7F, 0x00]) + body + bytes([checksum(body), 0xF7])


def make_request(dev, addr):
    return bytes([0xF0, 0x43, 0x20 | (dev & 0x0F), 0x7F, 0x00]) + bytes(addr) + b"\xf7"


def check(path):
    data = open(path, "rb").read()
    msgs = split_sysex(data)
    bad = 0
    n_bulk = 0
    for off, m, closed in msgs:
        if not closed:
            print("%06X  F7 で終わっていない（%d バイト）" % (off, len(m)))
            bad += 1
            continue
        if len(m) < 6 or m[1] != 0x43 or m[3:5] != b"\x7f\x00":
            print("%06X  別の種類のメッセージ: %s" % (off, m[:8].hex(" ")))
            continue
        kind = m[2] & 0xF0
        if kind != 0x00:
            print("%06X  バルクではない（%02X）: %s" % (off, m[2], m[:10].hex(" ")))
            continue
        n_bulk += 1
        body = m[5:-2]
        count = body[0] << 7 | body[1]
        addr = body[2:5]
        payload = body[5:]
        ok_len = count == len(payload)
        ok_sum = (sum(body) + m[-2]) & 0x7F == 0
        if any(b & 0x80 for b in body):
            ok_sum = False
        mark = "OK" if ok_len and ok_sum else "NG"
        if mark == "NG":
            bad += 1
        print("%06X  %s  番地 %02X %02X %02X  %4d バイト%s%s" % (
            off, mark, addr[0], addr[1], addr[2], len(payload),
            "" if ok_len else "（記録は %d）" % count, "" if ok_sum else "  チェックサムが合わない"))
    print("メッセージ %d 個、うちバルク %d 個、食い違い %d 個" % (len(msgs), n_bulk, bad))
    return 1 if bad else 0


def selftest():
    import os
    import tempfile
    m = make_bulk(0, (0x0E, 0x25, 0x00), bytes(range(0x30)))
    assert m[:5] == bytes([0xF0, 0x43, 0x00, 0x7F, 0x00]) and m[-1] == 0xF7
    assert (sum(m[5:-1])) & 0x7F == 0
    assert make_request(0, (0x0E, 0x25, 0x00)).hex() == "f043207f000e2500f7"
    broken = bytearray(m)
    broken[12] ^= 1
    fd, p = tempfile.mkstemp(suffix=".syx")
    os.write(fd, m + bytes(broken) + m[:-1])
    os.close(fd)
    try:
        assert len(split_sysex(open(p, "rb").read())) == 3
        assert check(p) == 1
    finally:
        os.remove(p)
    print("selftest: OK（上の NG 2 件は、わざと壊したもの）")


def main():
    a = sys.argv[1:]
    if not a:
        raise SystemExit(__doc__)
    if a[0] == "check":
        raise SystemExit(check(a[1]))
    if a[0] == "request":
        dev = 0
        if "--dev" in a:
            i = a.index("--dev")
            dev = int(a[i + 1])
            del a[i:i + 2]
        addr = [int(x, 16) for x in a[1:4]]
        if len(addr) != 3 or any(x > 0x7F for x in addr):
            raise SystemExit("番地は 7bit の 16 進を 3 つ")
        print(make_request(dev, addr).hex(" ").upper())
        return
    if a[0] == "selftest":
        selftest()
        return
    raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
