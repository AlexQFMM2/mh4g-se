#!/usr/bin/env python3
"""Export MH4G equipment calculation tables from a decompressed code.bin.

The script intentionally exports numeric save-local IDs only. Display names are
stored in core_common.arc and are joined by the normal data build pipeline.
ROM files and generated exports must remain outside Git.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
from pathlib import Path


KNOWN_CODE_SHA256 = "b3cf0ad1f9abca6cd007f5bb5d560b9302576f457bee9baaf2cdf12c0ce8156a"
VIRTUAL_ADDRESS_DELTA = 0x100000

ARMOR_TABLES = (
    ("head", 5, 0xE550D0, 987),
    ("chest", 1, 0xE5EB08, 972),
    ("arms", 2, 0xE682E8, 956),
    ("waist", 3, 0xE71848, 956),
    ("legs", 4, 0xE7ADA8, 964),
)

MELEE_WEAPON_TABLES = (
    ("great_sword", 7, 0xE42378, 246),
    ("sword_and_shield", 8, 0xE43A88, 247),
    ("hammer", 9, 0xE451B0, 247),
    ("lance", 10, 0xE468D8, 216),
    ("long_sword", 13, 0xE47D18, 227),
    ("switch_axe", 14, 0xE49260, 198),
    ("gunlance", 15, 0xE4A4F0, 221),
    ("dual_blades", 17, 0xE4B9A8, 229),
    ("hunting_horn", 18, 0xE4CF20, 185),
    ("insect_glaive", 19, 0xE4E078, 160),
    ("charge_blade", 20, 0xE4EF78, 125),
)

RANGED_WEAPON_TABLES = (
    ("heavy_bowgun", 12, 0xE4FB30, 157),
    ("light_bowgun", 11, 0xE513B8, 185),
    ("bow", 16, 0xE530A0, 206),
)

DECORATION_OFFSET = 0xE291FA
DECORATION_COUNT = 290


def signed8(value: int) -> int:
    return value if value < 0x80 else value - 0x100


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def table_slice(data: bytes, offset: int, record_size: int, count: int) -> bytes:
    end = offset + record_size * count
    if end > len(data):
        raise ValueError(f"table at 0x{offset:x} is truncated: need 0x{end:x}, file is 0x{len(data):x}")
    return data[offset:end]


def write_csv(path: Path, columns: list[str], rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def export_armor(data: bytes, output: Path) -> list[dict[str, object]]:
    manifest = []
    columns = [
        "save_id", "model_flags", "rarity", "special_flags", "base_defense", "price_value",
        "fire_resistance", "water_resistance", "thunder_resistance", "dragon_resistance",
        "ice_resistance", "slots",
    ]
    for index in range(1, 6):
        columns.extend((f"skill_{index}_id", f"skill_{index}_points"))
    columns.append("raw_hex")

    for part, save_type, offset, count in ARMOR_TABLES:
        raw_table = table_slice(data, offset, 40, count)
        rows = []
        for save_id in range(count):
            raw = raw_table[save_id * 40:(save_id + 1) * 40]
            row: dict[str, object] = {
                "save_id": save_id,
                "model_flags": raw[4],
                "rarity": 0 if save_id == 0 else raw[5] + 1,
                "special_flags": raw[6],
                "base_defense": raw[7],
                "price_value": struct.unpack_from("<I", raw, 8)[0],
                "fire_resistance": signed8(raw[12]),
                "water_resistance": signed8(raw[13]),
                "thunder_resistance": signed8(raw[14]),
                "dragon_resistance": signed8(raw[15]),
                "ice_resistance": signed8(raw[16]),
                "slots": raw[17],
                "raw_hex": raw.hex(),
            }
            for skill_index, skill_offset in enumerate(range(30, 40, 2), 1):
                row[f"skill_{skill_index}_id"] = raw[skill_offset]
                row[f"skill_{skill_index}_points"] = signed8(raw[skill_offset + 1])
            rows.append(row)
        write_csv(output / f"armor_{part}.csv", columns, rows)
        manifest.append({
            "name": f"armor_{part}",
            "save_type": save_type,
            "file_offset": f"0x{offset:08X}",
            "virtual_address": f"0x{offset + VIRTUAL_ADDRESS_DELTA:08X}",
            "record_size": 40,
            "record_count": count,
            "sha256": digest(raw_table),
        })
    return manifest


def export_decorations(data: bytes, output: Path) -> dict[str, object]:
    record_size = 8
    raw_table = table_slice(data, DECORATION_OFFSET, record_size, DECORATION_COUNT)
    rows = []
    for save_id in range(DECORATION_COUNT):
        raw = raw_table[save_id * record_size:(save_id + 1) * record_size]
        rows.append({
            "save_id": save_id,
            "item_save_id": struct.unpack_from("<H", raw, 0)[0],
            "slots": raw[2],
            "skill_1_id": raw[3],
            "skill_1_points": signed8(raw[4]),
            "skill_2_id": raw[5],
            "skill_2_points": signed8(raw[6]),
            "flags": raw[7],
            "raw_hex": raw.hex(),
        })
    write_csv(output / "decorations.csv", list(rows[0]), rows)
    return {
        "name": "decorations",
        "file_offset": f"0x{DECORATION_OFFSET:08X}",
        "virtual_address": f"0x{DECORATION_OFFSET + VIRTUAL_ADDRESS_DELTA:08X}",
        "record_size": record_size,
        "record_count": DECORATION_COUNT,
        "sha256": digest(raw_table),
    }


def export_weapons(data: bytes, output: Path) -> list[dict[str, object]]:
    columns = [
        "weapon_type", "save_type", "save_id", "layout", "rarity", "attack_raw", "defense",
        "affinity_percent", "slots", "special_1_id", "special_1_value",
        "special_2_id", "special_2_value", "is_relic", "raw_hex",
    ]
    rows = []
    manifest = []
    for weapon_type, save_type, offset, count in MELEE_WEAPON_TABLES:
        record_size = 24
        raw_table = table_slice(data, offset, record_size, count)
        for save_id in range(count):
            raw = raw_table[save_id * record_size:(save_id + 1) * record_size]
            rows.append({
                "weapon_type": weapon_type,
                "save_type": save_type,
                "save_id": save_id,
                "layout": "melee",
                "rarity": 0 if save_id == 0 else raw[19] + 1,
                "attack_raw": struct.unpack_from("<H", raw, 8)[0],
                "defense": raw[10],
                "affinity_percent": signed8(raw[11]),
                "slots": raw[16],
                "special_1_id": raw[12],
                "special_1_value": signed8(raw[13]) * 10,
                "special_2_id": raw[14],
                "special_2_value": signed8(raw[15]) * 10,
                "is_relic": int(bool(raw[22] & 0x80)),
                "raw_hex": raw.hex(),
            })
        manifest.append({
            "name": f"weapon_{weapon_type}",
            "save_type": save_type,
            "file_offset": f"0x{offset:08X}",
            "virtual_address": f"0x{offset + VIRTUAL_ADDRESS_DELTA:08X}",
            "record_size": record_size,
            "record_count": count,
            "sha256": digest(raw_table),
        })

    for weapon_type, save_type, offset, count in RANGED_WEAPON_TABLES:
        record_size = 40
        raw_table = table_slice(data, offset, record_size, count)
        for save_id in range(count):
            raw = raw_table[save_id * record_size:(save_id + 1) * record_size]
            rows.append({
                "weapon_type": weapon_type,
                "save_type": save_type,
                "save_id": save_id,
                "layout": "ranged",
                "rarity": 0 if save_id == 0 else raw[4] + 1,
                "attack_raw": struct.unpack_from("<H", raw, 12)[0],
                "defense": raw[14],
                "affinity_percent": signed8(raw[17]),
                "slots": raw[16],
                "special_1_id": "",
                "special_1_value": "",
                "special_2_id": "",
                "special_2_value": "",
                "is_relic": int(bool(raw[28] & 0x80)),
                "raw_hex": raw.hex(),
            })
        manifest.append({
            "name": f"weapon_{weapon_type}",
            "save_type": save_type,
            "file_offset": f"0x{offset:08X}",
            "virtual_address": f"0x{offset + VIRTUAL_ADDRESS_DELTA:08X}",
            "record_size": record_size,
            "record_count": count,
            "sha256": digest(raw_table),
        })

    write_csv(output / "weapons.csv", columns, rows)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("code_bin", type=Path, help="decompressed MH4G code.bin")
    parser.add_argument("output", type=Path, help="output directory outside Git")
    parser.add_argument("--allow-unknown-hash", action="store_true")
    args = parser.parse_args()

    data = args.code_bin.read_bytes()
    code_hash = digest(data)
    if code_hash != KNOWN_CODE_SHA256 and not args.allow_unknown_hash:
        raise SystemExit(
            f"unsupported code.bin SHA-256: {code_hash}; expected {KNOWN_CODE_SHA256}. "
            "Use --allow-unknown-hash only for research."
        )

    args.output.mkdir(parents=True, exist_ok=True)
    tables = []
    tables.extend(export_armor(data, args.output))
    tables.append(export_decorations(data, args.output))
    tables.extend(export_weapons(data, args.output))
    manifest = {
        "format": "mh4g-native-equipment-export-v1",
        "source": "decompressed-code.bin",
        "code_sha256": code_hash,
        "virtual_address_delta": f"0x{VIRTUAL_ADDRESS_DELTA:X}",
        "tables": tables,
    }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"exported {len(tables)} tables to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
