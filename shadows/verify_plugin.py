"""Check the staged Shadows plugin's record links and alias/objective contract."""
import json
from pathlib import Path
import runpy
import struct
import sys

parser = runpy.run_path(str(Path(__file__).resolve().parents[1] / "tests/persistence-spike/verify_plugin.py"))
records, subrecords, require = (parser[k] for k in ("records", "subrecords", "require"))


def verify(path):
    parsed = list(records(Path(path).read_bytes()))
    header = list(subrecords(parsed[0][2]))
    require([v.rstrip(b"\0") for k, v in header if k == b"MAST"] == [b"Starfield.esm"], "Unexpected masters")
    expected = {(b"QUST", 0x01000800), (b"QUST", 0x01000801), (b"MESG", 0x01000802)}
    expected |= {(b"SPEL", i) for i in range(0x01000803, 0x01000806)}
    expected |= {(b"MGEF", i) for i in range(0x01000806, 0x01000809)}
    require(len(parsed) == 10 and {(k, i) for k, i, _ in parsed[1:]} == expected, "Missing/extra records or foreign overrides")
    fields = {i: list(subrecords(body)) for _, i, body in parsed[1:]}
    hunt = fields[0x01000801]
    require([struct.unpack("<I", v)[0] for k, v in hunt if k == b"ALST"] == [0, 1], "Wrong alias slots")
    require([struct.unpack("<I", v)[0] for k, v in hunt if k == b"FNAM"] == [0x112, 0x2], "Nemesis requires StoresText; weapon must not be a quest object")
    require([struct.unpack("<I", v)[0] for k, v in hunt if k == b"ALDN"] == [0x01000802], "Wrong alias title")
    require([struct.unpack("<H", v)[0] for k, v in hunt if k == b"QOBJ"] == [10], "Wrong hunt objective")
    require([struct.unpack_from("<i", v)[0] for k, v in hunt if k == b"QSTA"] == [0], "Objective must point to the actual nemesis")
    for i, script, props in [(0x01000800, "OSFDefeatShadowsAftermath", ["Hunt"]),
                             (0x01000801, "OSFDefeatShadowsHunt", ["Nemesis", "StolenWeapon", "NemesisBuff", "PlayerDebuff", "PlayerReward"])]:
        vmad = dict(fields[i])[b"VMAD"]
        for name in [script] + props:
            token = name.encode()
            require(struct.pack("<H", len(token)) + token in vmad, "Missing script binding: " + name)
    for i, magnitude in [(0x01000803, 40.0), (0x01000804, 20.0), (0x01000805, 10.0)]:
        spell = dict(fields[i])
        require(struct.unpack("<I", spell[b"EFID"])[0] == i + 3, "Wrong trait effect link")
        require(struct.unpack_from("<f", spell[b"EFIT"])[0] == magnitude, "Wrong trait magnitude")
    return {"outcome": "passed", "records": 9, "nemesisSlots": 1, "runtimeVerified": False}


if __name__ == "__main__":
    print(json.dumps(verify(sys.argv[1]), indent=2))
