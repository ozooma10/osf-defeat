"""Scan a Starfield ESM for records of given types, printing FormID + EditorID.

Walks the TES4 top level linearly: GRUP headers are 24 bytes and their contents
follow immediately, so descending is just skipping the header. Record headers are
24 bytes: Type(4) DataSize(u32) Flags(u32) FormID(u32) Revision(u32) Version(u16) Unk(u16).
Compressed records (flag 0x00040000) hold [decompSize u32][zlib stream].
"""

import mmap
import os
import re
import struct
import sys
import zlib

ESM = r"C:\Program Files (x86)\Steam\steamapps\common\Starfield\Data\Starfield.esm"
# Default record types to scan; override with OSF_SCAN_TYPES (comma-separated, e.g. "KYWD").
TARGETS = {t.encode() for t in os.environ.get("OSF_SCAN_TYPES", "SPEL,FACT").split(",") if t}
COMPRESSED = 0x00040000


def edid_of(data: bytes) -> str | None:
    pos = 0
    n = len(data)
    while pos + 6 <= n:
        stype = data[pos : pos + 4]
        ssize = struct.unpack_from("<H", data, pos + 4)[0]
        if stype == b"XXXX":
            real = struct.unpack_from("<I", data, pos + 6)[0]
            pos += 6 + ssize  # move past XXXX subrecord
            # next subrecord uses the XXXX size; its own size field is 0
            pos += 6 + real
            continue
        if stype == b"EDID":
            return data[pos + 6 : pos + 6 + ssize].rstrip(b"\0").decode("cp1252", "replace")
        pos += 6 + ssize
    return None


def main() -> None:
    pattern = re.compile(sys.argv[1], re.I) if len(sys.argv) > 1 else None
    out: list[tuple[str, int, str]] = []
    with open(ESM, "rb") as fh, mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ) as mm:
        n = len(mm)
        # skip the TES4 header record
        pos = 24 + struct.unpack_from("<I", mm, 4)[0]
        while pos + 24 <= n:
            rtype = mm[pos : pos + 4]
            if rtype == b"GRUP":
                pos += 24  # descend
                continue
            data_size = struct.unpack_from("<I", mm, pos + 4)[0]
            if rtype in TARGETS:
                flags, form_id = struct.unpack_from("<II", mm, pos + 8)
                data = mm[pos + 24 : pos + 24 + data_size]
                if flags & COMPRESSED:
                    try:
                        data = zlib.decompress(data[4:])
                    except zlib.error:
                        data = b""
                edid = edid_of(data)
                if edid and (pattern is None or pattern.search(edid)):
                    out.append((rtype.decode(), form_id, edid))
            pos += 24 + data_size

    for rtype, form_id, edid in sorted(out):
        print(f"{rtype} 0x{form_id:08X} {edid}")
    print(f"-- {len(out)} match(es)", file=sys.stderr)


if __name__ == "__main__":
    main()
