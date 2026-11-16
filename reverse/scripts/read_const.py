"""Read constants at virtual addresses of a thin x86_64 Mach-O (the ERA 4 slices Ghidra used).

    python3 -I read_const.py <binary> <type> <addr> [<addr> ...]
    type: d (double), f (float), i (int32), q (int64), s (C string), x16 (16 raw hex bytes)
    e.g.  read_const.py ~/Library/Caches/era-ghidra/bin/ERA4_PlosiveRemover d 0xb19618 0xb53550

Addresses are the ones in Ghidra's pseudo-C (DAT_00b19618 -> 0xb19618).
"""
import struct, sys

def segments(data):
    magic, _, _, _, ncmds, _, _, _ = struct.unpack_from("<IiiIIIII", data, 0)
    assert magic == 0xFEEDFACF, "not a thin 64-bit Mach-O (lipo -thin x86_64 first)"
    off = 32
    for _ in range(ncmds):
        cmd, size = struct.unpack_from("<II", data, off)
        if cmd == 0x19:  # LC_SEGMENT_64
            vmaddr, vmsize, fileoff, filesize = struct.unpack_from("<QQQQ", data, off + 24)
            yield vmaddr, vmsize, fileoff, filesize
        off += size

def read(data, segs, addr, n):
    for vm, vs, fo, fs in segs:
        if vm <= addr < vm + vs:
            o = fo + addr - vm
            return data[o:o + n] if addr - vm + n <= fs else None
    return None

def main():
    path, kind, *addrs = sys.argv[1:]
    data = open(path, "rb").read()
    segs = list(segments(data))
    fmt = {"d": ("<d", 8), "f": ("<f", 4), "i": ("<i", 4), "q": ("<q", 8)}
    for a in addrs:
        addr = int(a, 16)
        if kind == "s":
            raw = read(data, segs, addr, 256) or b""
            val = raw.split(b"\0")[0].decode("latin1")
        elif kind == "x16":
            raw = read(data, segs, addr, 16)
            val = raw.hex(" ") if raw else None
        else:
            f, n = fmt[kind]
            raw = read(data, segs, addr, n)
            val = struct.unpack(f, raw)[0] if raw else None
        print(f"{a}: {val}")

if __name__ == "__main__":
    main()
