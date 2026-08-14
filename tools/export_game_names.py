#!/usr/bin/env python3
"""Export authoritative save-ID name arrays from MH4G core_common.arc.

The output contains only decoded message strings and source hashes. Raw CCI,
RomFS, ARC, and LMD files stay outside the repository.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zlib
from pathlib import Path


FORMAT = "mh4g-game-name-export-v1"
ARC_MAGIC = b"ARC\0"
LMD_MAGIC = b"lmd\0"
ARC_HEADER_SIZE = 12
ARC_ENTRY_SIZE = 80
ARC_NAME_SIZE = 64
AUTHORITATIVE_TABLES = {
    "msg\\itemName_jpn",
    "msg\\BodyName_jpn", "msg\\ArmName_jpn", "msg\\WaistName_jpn",
    "msg\\LegName_jpn", "msg\\HeadName_jpn",
    "msg\\LswordName_jpn", "msg\\SwordName_jpn", "msg\\HammerName_jpn",
    "msg\\LanceName_jpn", "msg\\LightName_jpn", "msg\\HeavyName_jpn",
    "msg\\Lsword2Name_jpn", "msg\\AxeName_jpn", "msg\\Lance2Name_jpn",
    "msg\\BowName_jpn", "msg\\WswordName_jpn", "msg\\Hammer2Name_jpn",
    "msg\\RodName_jpn", "msg\\GaxeName_jpn",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_arc(path: Path) -> dict[str, bytes]:
    data = path.read_bytes()
    if data[:4] != ARC_MAGIC:
        raise ValueError(f"{path}: not an MT Framework ARC archive")
    version, count = struct.unpack_from("<HH", data, 4)
    if version != 0x13:
        raise ValueError(f"{path}: unsupported ARC version 0x{version:04x}")
    table_end = ARC_HEADER_SIZE + count * ARC_ENTRY_SIZE
    if table_end > len(data):
        raise ValueError(f"{path}: truncated ARC entry table")

    result: dict[str, bytes] = {}
    for index in range(count):
        entry = ARC_HEADER_SIZE + index * ARC_ENTRY_SIZE
        raw_name = data[entry:entry + ARC_NAME_SIZE].split(b"\0", 1)[0]
        name = raw_name.decode("ascii", errors="strict")
        compressed_size, size_flags, offset = struct.unpack_from(
            "<III", data, entry + ARC_NAME_SIZE + 4
        )
        # core_common.arc contains duplicate non-message resource paths. They
        # are irrelevant to this exporter; only the save-ID name tables are
        # decoded, while duplicate authoritative tables remain a hard error.
        if name not in AUTHORITATIVE_TABLES:
            continue
        if name in result:
            raise ValueError(f"{path}: duplicate authoritative ARC entry {name}")

        expected_size = size_flags & 0x00FFFFFF
        end = offset + compressed_size
        if end > len(data):
            raise ValueError(f"{path}: {name} payload is outside the archive")
        try:
            payload = zlib.decompress(data[offset:end])
        except zlib.error as exc:
            raise ValueError(f"{path}: cannot decompress {name}: {exc}") from exc
        if len(payload) != expected_size:
            raise ValueError(
                f"{path}: {name} size mismatch: expected {expected_size}, got {len(payload)}"
            )
        result[name] = payload
    return result


def read_lmd(data: bytes, label: str) -> list[str]:
    if data[:4] != LMD_MAGIC:
        raise ValueError(f"{label}: not an LMD message table")
    if len(data) < 36:
        raise ValueError(f"{label}: truncated LMD header")
    count = struct.unpack_from("<I", data, 8)[0]
    records_offset = struct.unpack_from("<I", data, 28)[0]
    records_end = records_offset + count * 12
    if records_end > len(data):
        raise ValueError(f"{label}: truncated LMD string records")

    result: list[str] = []
    for index in range(count):
        offset, length, capacity = struct.unpack_from("<III", data, records_offset + index * 12)
        if length > capacity:
            raise ValueError(f"{label}: string {index} length exceeds capacity")
        end = offset + length * 2
        if end > len(data):
            raise ValueError(f"{label}: string {index} is outside the LMD payload")
        value = data[offset:end].decode("utf-16le", errors="strict")
        if not value:
            raise ValueError(f"{label}: string {index} is empty")
        result.append(value)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("core_common", type=Path, help="extracted MH4G data/core_common.arc")
    parser.add_argument("output", type=Path, help="output JSON outside the repository")
    parser.add_argument(
        "--language",
        required=True,
        choices=("cn", "en", "jp"),
        help="language actually contained in the *_jpn message entries",
    )
    args = parser.parse_args()

    source = args.core_common.resolve()
    archive = read_arc(source)
    missing = AUTHORITATIVE_TABLES - set(archive)
    if missing:
        raise ValueError(f"core_common.arc is missing required MH4G name tables: {sorted(missing)}")
    tables = {name: read_lmd(archive[name], name) for name in sorted(AUTHORITATIVE_TABLES)}

    payload = {
        "format": FORMAT,
        "language": args.language,
        "source": {
            "file": source.name,
            "size": source.stat().st_size,
            "sha256": sha256(source),
        },
        "tables": tables,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
