#!/usr/bin/env python3
"""Check a built Fritz Dash PKG without any PS4 tools.

Reads the PKG header (magic, content ID), finds the PARAM_SFO entry in the
entry table, parses param.sfo, and compares every field with the expected
values. Exits non-zero on any mismatch.

Usage: python3 verify_pkg.py <file.pkg>
"""
import hashlib
import struct
import sys
from pathlib import Path

EXPECTED = {
    "TITLE_ID": "FRTZ00001",
    "CONTENT_ID": "IV0000-FRTZ00001_00-FRITZDASH0000000",
    "CATEGORY": "gd",
    "APP_VER": "03.20",
    "VERSION": "03.20",
    "TITLE": "Fritz Dash",
    "APP_TYPE": 1,
}
PARAM_SFO_ID = 0x1000


def parse_sfo(data):
    magic, _version, key_start, data_start, count = struct.unpack_from("<4sIIII", data, 0)
    if magic != b"\x00PSF":
        raise ValueError(f"bad SFO magic {magic!r}")
    fields = {}
    for i in range(count):
        key_off, fmt, length, _max_len, data_off = struct.unpack_from("<HHIII", data, 20 + i * 16)
        key_end = data.index(b"\x00", key_start + key_off)
        key = data[key_start + key_off:key_end].decode()
        raw = data[data_start + data_off:data_start + data_off + length]
        if fmt == 0x0404:
            fields[key] = struct.unpack("<I", raw)[0]
        else:
            fields[key] = raw.rstrip(b"\x00").decode()
    return fields


def main():
    path = Path(sys.argv[1])
    data = path.read_bytes()
    ok = True

    magic = data[:4]
    print(f"magic: {magic!r}", "OK" if magic == b"\x7fCNT" else "BAD")
    ok &= magic == b"\x7fCNT"

    content_id = data[0x40:0x40 + 36].decode(errors="replace")
    good = content_id == EXPECTED["CONTENT_ID"]
    print(f"header content ID: {content_id}", "OK" if good else "BAD")
    ok &= good

    entry_count, = struct.unpack_from(">I", data, 0x10)
    table_off, = struct.unpack_from(">I", data, 0x18)
    sfo = None
    for i in range(entry_count):
        ent_id, _name, flags1, _flags2, off, size = struct.unpack_from(">IIIIII", data, table_off + i * 32)
        if ent_id == PARAM_SFO_ID:
            if flags1 & 0x80000000:
                raise ValueError("param.sfo entry is encrypted")
            sfo = parse_sfo(data[off:off + size])
    if sfo is None:
        print("PARAM_SFO entry: missing BAD")
        sys.exit(1)

    for key, want in EXPECTED.items():
        got = sfo.get(key)
        good = got == want
        ok &= good
        print(f"param.sfo {key} = {got!r}", "OK" if good else f"BAD (want {want!r})")

    print(f"size: {len(data)} bytes")
    print(f"sha256: {hashlib.sha256(data).hexdigest()}")
    print("VERIFY OK" if ok else "VERIFY FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
