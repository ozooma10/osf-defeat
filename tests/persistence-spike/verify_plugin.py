"""Inspect the built ESM, independently of Spriggit. This is NOT runtime evidence."""
import json
from pathlib import Path
import struct
import sys
import zlib


def require(condition, message):
    if not condition:
        raise ValueError(message)


def records(data, start=0, end=None):
    end = len(data) if end is None else end
    pos = start
    while pos < end:
        require(pos + 24 <= end, "Truncated record header")
        kind = data[pos:pos + 4]
        size, flags, form_id = struct.unpack_from("<III", data, pos + 4)
        if kind == b"GRUP":
            require(size >= 24 and pos + size <= end, "Invalid group size")
            yield from records(data, pos + 24, pos + size)
            pos += size
            continue
        require(pos + 24 + size <= end, "Truncated record")
        body = data[pos + 24:pos + 24 + size]
        if flags & 0x40000:
            expected, = struct.unpack_from("<I", body)
            body = zlib.decompress(body[4:])
            require(len(body) == expected, "Compressed record size mismatch")
        yield kind, form_id, body
        pos += 24 + size


def subrecords(data):
    pos = 0
    while pos < len(data):
        require(pos + 6 <= len(data), "Truncated subrecord")
        kind = data[pos:pos + 4]
        size, = struct.unpack_from("<H", data, pos + 4)
        pos += 6
        if kind == b"XXXX":
            require(size == 4 and pos + 10 <= len(data), "Invalid extended size")
            size, = struct.unpack_from("<I", data, pos)
            kind = data[pos + 4:pos + 8]
            pos += 10
        require(pos + size <= len(data), "Truncated subrecord body")
        yield kind, data[pos:pos + size]
        pos += size


def verify(path):
    parsed = list(records(Path(path).read_bytes()))
    require(parsed[0][0] == b"TES4", "Missing plugin header")
    header = list(subrecords(parsed[0][2]))
    require([v.rstrip(b"\0") for k, v in header if k == b"MAST"] == [b"Starfield.esm"], "Unexpected masters")
    expected = {(b"QUST", 0x01000800), (b"MESG", 0x01000801), (b"MESG", 0x01000802)}
    require(len(parsed) == 4 and {(k, i) for k, i, _ in parsed[1:]} == expected, "Missing/extra records or foreign overrides")
    quest = list(subrecords(next(body for k, _, body in parsed if k == b"QUST")))
    require(next(v for k, v in quest if k == b"EDID") == b"OSFDefeatPersistenceSpike\0", "Wrong quest")
    require(struct.unpack_from("<I", next(v for k, v in quest if k == b"DNAM"))[0] == 0, "Probe must not autostart")
    require([struct.unpack("<I", v)[0] for k, v in quest if k == b"ALST"] == [0, 1, 2], "Wrong alias inventory")
    require([v.rstrip(b"\0") for k, v in quest if k == b"ALID"] == [b"Subject", b"Pack", b"ShipMarker"], "Wrong alias names")
    require([struct.unpack("<I", v)[0] for k, v in quest if k == b"ALDN"] == [0x01000801], "Alias title missing/wrong")
    require([struct.unpack("<H", v)[0] for k, v in quest if k == b"QOBJ"] == [10, 20], "Wrong objectives")
    require([struct.unpack_from("<i", v)[0] for k, v in quest if k == b"QSTA"] == [0, 1], "Wrong objective targets")
    vmad = next(v for k, v in quest if k == b"VMAD")
    for name in (b"OSFDefeatPersistenceSpike", b"SubjectAlias", b"PackAlias", b"ShipMarkerAlias", b"SubjectBase", b"PackBase", b"OverrideTitle"):
        require(struct.pack("<H", len(name)) + name in vmad, "Missing script binding: " + name.decode())
    for form_id, title in ((0x01000801, b"Persistence Subject"), (0x01000802, b"Persistence Override")):
        fields = dict(subrecords(next(body for _, i, body in parsed if i == form_id)))
        require(fields.get(b"FULL", b"").rstrip(b"\0") == title, "Wrong Message title")
    return {"outcome": "passed", "records": 3, "aliases": 3, "objectives": 2, "runtimeVerified": False}


if __name__ == "__main__":
    print(json.dumps(verify(sys.argv[1]), indent=2))
