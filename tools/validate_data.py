#!/usr/bin/env python3
"""Validate the generated MH4G dataset and optional real save samples."""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import struct
import subprocess
import tempfile
import warnings
from collections import Counter
from pathlib import Path
from typing import Iterable


SAVE_SIZE = 81_408
ITEM_OFFSET = 0x015E
ITEM_SLOTS = 1_400
EQUIPMENT_OFFSET = 0x173E
EQUIPMENT_SLOTS = 1_500
EQUIPMENT_SIZE = 28
BLOWFISH_KEY = b"blowfish key iorajegqmrna4itjeangmb agmwgtobjteowhv9mope"

BASE_COLUMNS = ["id", "name", "english", "source"]
EQUIPMENT_COLUMNS = BASE_COLUMNS + ["rarity", "is_relic"]
LOOKUP_COLUMNS = ["domain", "equipment_type", "variant", "value", "name", "english", "source"]


class ValidationErrors:
    def __init__(self) -> None:
        self.messages: list[str] = []

    def add(self, message: str) -> None:
        self.messages.append(message)

    def require(self, condition: bool, message: str) -> None:
        if not condition:
            self.add(message)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_csv(path: Path, errors: ValidationErrors) -> tuple[list[str], list[dict[str, str]]]:
    raw = path.read_bytes()
    errors.require(not raw.startswith(b"\xef\xbb\xbf"), f"{path}: UTF-8 BOM is not allowed")
    errors.require(b"\r" not in raw, f"{path}: only LF line endings are allowed")
    try:
        text = raw.decode("utf-8", errors="strict")
    except UnicodeDecodeError as exc:
        errors.add(f"{path}: invalid UTF-8: {exc}")
        return [], []
    reader = csv.DictReader(io.StringIO(text, newline=""))
    try:
        rows = list(reader)
    except csv.Error as exc:
        errors.add(f"{path}: invalid CSV: {exc}")
        return list(reader.fieldnames or []), []
    return list(reader.fieldnames or []), rows


def parse_decimal(value: str, label: str, errors: ValidationErrors) -> int | None:
    if not value or not value.isdecimal():
        errors.add(f"{label}: expected an unsigned decimal integer, got {value!r}")
        return None
    return int(value, 10)


def validate_entity(
    relative: str,
    columns: list[str],
    rows: list[dict[str, str]],
    equipment: bool,
    errors: ValidationErrors,
) -> list[int]:
    expected = EQUIPMENT_COLUMNS if equipment else BASE_COLUMNS
    errors.require(columns == expected, f"{relative}: columns must be {expected}, got {columns}")
    identifiers: list[int] = []
    for line, row in enumerate(rows, 2):
        identifier = parse_decimal(row.get("id", ""), f"{relative}:{line}: id", errors)
        if identifier is not None:
            identifiers.append(identifier)
        for column in ("name", "english", "source"):
            errors.require(bool(row.get(column, "").strip()), f"{relative}:{line}: {column} must not be empty")
        if equipment:
            parse_decimal(row.get("rarity", ""), f"{relative}:{line}: rarity", errors)
            relic = parse_decimal(row.get("is_relic", ""), f"{relative}:{line}: is_relic", errors)
            if relic is not None:
                errors.require(relic in (0, 1), f"{relative}:{line}: is_relic must be 0 or 1")
    errors.require(len(identifiers) == len(set(identifiers)), f"{relative}: duplicate IDs")
    errors.require(identifiers == sorted(identifiers), f"{relative}: IDs are not numerically sorted")
    return identifiers


def validate_lookups(
    relative: str,
    columns: list[str],
    rows: list[dict[str, str]],
    errors: ValidationErrors,
) -> list[tuple[str, int, str, int]]:
    errors.require(columns == LOOKUP_COLUMNS, f"{relative}: columns must be {LOOKUP_COLUMNS}, got {columns}")
    keys: list[tuple[str, int, str, int]] = []
    for line, row in enumerate(rows, 2):
        equipment_type = parse_decimal(row.get("equipment_type", ""), f"{relative}:{line}: equipment_type", errors)
        value = parse_decimal(row.get("value", ""), f"{relative}:{line}: value", errors)
        for column in ("domain", "variant", "name", "english", "source"):
            errors.require(bool(row.get(column, "").strip()), f"{relative}:{line}: {column} must not be empty")
        if equipment_type is not None:
            errors.require(0 <= equipment_type <= 20, f"{relative}:{line}: equipment_type outside 0..20")
        if equipment_type is not None and value is not None:
            keys.append((row["domain"], equipment_type, row["variant"], value))
    errors.require(len(keys) == len(set(keys)), f"{relative}: duplicate domain/type/variant/value keys")
    errors.require(keys == sorted(keys), f"{relative}: lookup rows are not deterministically sorted")
    return keys


def expected_files(mapping: dict) -> set[str]:
    entities = {
        "items.csv",
        "skills.csv",
        "decorations.csv",
        "equipment_types.csv",
        "talismans.csv",
        "equipment_lookups.csv",
    }
    entities.update(spec["file"] for spec in mapping["equipment"])
    return entities


def swap_dwords(data: bytearray) -> None:
    for offset in range(0, len(data), 4):
        data[offset:offset + 4] = data[offset:offset + 4][::-1]


def blowfish_decrypt(ciphertext: bytes) -> bytes:
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
            decryptor = Cipher(algorithms.Blowfish(BLOWFISH_KEY), modes.ECB()).decryptor()
            return decryptor.update(ciphertext) + decryptor.finalize()
    except (ImportError, AttributeError):
        pass

    with tempfile.TemporaryDirectory(prefix="mh4g-save-decrypt-") as directory:
        root = Path(directory)
        source = root / "encrypted.bin"
        target = root / "decrypted.bin"
        source.write_bytes(ciphertext)
        command = [
            "openssl", "enc", "-bf-ecb", "-d", "-nopad", "-provider", "legacy",
            "-K", BLOWFISH_KEY.hex(), "-in", str(source), "-out", str(target),
        ]
        completed = subprocess.run(command, capture_output=True, text=True, check=False)
        if completed.returncode != 0:
            raise RuntimeError("Blowfish support requires Python cryptography or OpenSSL legacy provider: " + completed.stderr.strip())
        return target.read_bytes()


def decrypt_save(path: Path) -> bytes:
    encrypted = bytearray(path.read_bytes())
    if len(encrypted) != SAVE_SIZE:
        raise ValueError(f"expected {SAVE_SIZE} bytes, got {len(encrypted)}")
    swap_dwords(encrypted)
    decrypted = bytearray(blowfish_decrypt(bytes(encrypted)))
    swap_dwords(decrypted)
    key = struct.unpack_from("<H", decrypted, 2)[0]
    for offset in range(4, len(decrypted), 2):
        if key == 0:
            key = 1
        key = key * 0xB0 % 0xFF53
        value = struct.unpack_from("<H", decrypted, offset)[0] ^ key
        struct.pack_into("<H", decrypted, offset, value)
    if struct.unpack_from("<H", decrypted, 0)[0] != 16:
        raise ValueError("decrypted save header is invalid")
    stored_checksum = struct.unpack_from("<I", decrypted, 4)[0]
    actual_checksum = sum(decrypted[8:]) & 0xFFFFFFFF
    if stored_checksum != actual_checksum:
        raise ValueError(f"checksum mismatch: stored={stored_checksum}, actual={actual_checksum}")
    return bytes(decrypted)


def load_id_sets(data_dir: Path, mapping: dict) -> tuple[set[int], set[int], dict[int, set[int]]]:
    def ids(relative: str) -> set[int]:
        with (data_dir / "en" / relative).open("r", encoding="utf-8", newline="") as handle:
            return {int(row["id"]) for row in csv.DictReader(handle)}

    equipment = {0: {0}, 6: ids("talismans.csv")}
    for spec in mapping["equipment"]:
        equipment[spec["save_type"]] = ids(spec["file"])
    return ids("items.csv"), ids("decorations.csv"), equipment


def validate_samples(data_dir: Path, mapping: dict, sample_dir: Path, errors: ValidationErrors) -> dict[str, object]:
    item_ids, decoration_ids, equipment_ids = load_id_sets(data_dir, mapping)
    save_paths = sorted(path for path in sample_dir.iterdir() if path.is_file() and path.stat().st_size == SAVE_SIZE)
    errors.require(len(save_paths) >= 3, f"{sample_dir}: expected at least three {SAVE_SIZE}-byte save samples")
    report: dict[str, object] = {"sample_directory": str(sample_dir), "saves": [], "unmatched": []}
    unmatched: list[dict[str, object]] = []
    for path in save_paths:
        try:
            decrypted = decrypt_save(path)
        except Exception as exc:
            errors.add(f"{path}: decrypt failed: {exc}")
            continue
        logical = memoryview(decrypted)[8:]
        nonempty_items = 0
        type_counts: Counter[int] = Counter()
        last_100 = 0
        decorations = 0
        for slot in range(ITEM_SLOTS):
            item_id, quantity = struct.unpack_from("<HH", logical, ITEM_OFFSET + slot * 4)
            if item_id == 0 and quantity == 0:
                continue
            nonempty_items += 1
            if item_id not in item_ids:
                unmatched.append({"save": path.name, "section": "items", "slot": slot, "id": item_id, "quantity": quantity})
        for slot in range(EQUIPMENT_SLOTS):
            offset = EQUIPMENT_OFFSET + slot * EQUIPMENT_SIZE
            equipment_type = logical[offset]
            equipment_id = struct.unpack_from("<H", logical, offset + 2)[0]
            if equipment_type == 0:
                continue
            type_counts[equipment_type] += 1
            if slot >= 1_400:
                last_100 += 1
            if equipment_type not in equipment_ids or equipment_id not in equipment_ids[equipment_type]:
                unmatched.append({"save": path.name, "section": "equipment", "slot": slot, "type": equipment_type, "id": equipment_id})
            for decoration_offset in (6, 8, 10):
                decoration_id = struct.unpack_from("<H", logical, offset + decoration_offset)[0] & 0x7FFF
                if decoration_id in (0, 0x7FFF):
                    continue
                decorations += 1
                if decoration_id not in decoration_ids:
                    unmatched.append({
                        "save": path.name,
                        "section": "decorations",
                        "slot": slot,
                        "equipment_type": equipment_type,
                        "id": decoration_id,
                    })
        report["saves"].append({
            "file": path.name,
            "nonempty_item_slots": nonempty_items,
            "nonempty_equipment_slots": sum(type_counts.values()),
            "nonempty_last_100_equipment_slots": last_100,
            "decoration_references": decorations,
            "equipment_type_counts": {str(key): type_counts[key] for key in sorted(type_counts)},
        })
    report["unmatched"] = unmatched
    errors.require(not unmatched, f"real save coverage has {len(unmatched)} unmatched records; see validation report")
    return report


def validate(data_dir: Path, mapping_path: Path, samples: Path | None, report_path: Path | None) -> tuple[ValidationErrors, dict[str, object]]:
    errors = ValidationErrors()
    mapping = json.loads(mapping_path.read_text(encoding="utf-8"))
    manifest_path = data_dir / "manifest.json"
    errors.require(manifest_path.is_file(), f"{manifest_path}: missing")
    if not manifest_path.is_file():
        return errors, {}
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    errors.require(manifest.get("format_version") == "1.0.0", "manifest: unsupported format_version")
    errors.require(manifest.get("languages") == ["cn", "en"], "manifest: languages must be ['cn', 'en']")
    errors.require(manifest.get("generator", {}).get("version") == "1.0.0", "manifest: unsupported generator version")
    errors.require("timestamp" not in json.dumps(manifest).lower(), "manifest: timestamps are forbidden")
    for source_file in manifest.get("dex", {}).get("source_files", []):
        digest = source_file.get("sha256", "")
        errors.require(len(digest) == 64 and all(char in "0123456789abcdef" for char in digest), f"manifest: invalid Dex SHA-256 for {source_file.get('name')}")

    wanted = expected_files(mapping)
    language_data: dict[str, dict[str, tuple[list[str], list[dict[str, str]], list[object]]]] = {"cn": {}, "en": {}}
    equipment_files = {"talismans.csv"} | {spec["file"] for spec in mapping["equipment"]}
    for language in ("cn", "en"):
        language_dir = data_dir / language
        actual = {path.name for path in language_dir.glob("*.csv")} if language_dir.is_dir() else set()
        errors.require(actual == wanted, f"{language}: file set mismatch; missing={sorted(wanted-actual)}, extra={sorted(actual-wanted)}")
        for filename in sorted(wanted & actual):
            relative = f"{language}/{filename}"
            columns, rows = load_csv(language_dir / filename, errors)
            if filename == "equipment_lookups.csv":
                keys: list[object] = validate_lookups(relative, columns, rows, errors)
            else:
                keys = validate_entity(relative, columns, rows, filename in equipment_files, errors)
            if language == "en":
                for line, row in enumerate(rows, 2):
                    errors.require(row.get("name") == row.get("english"), f"{relative}:{line}: English name and english columns differ")
            language_data[language][filename] = (columns, rows, keys)
            entry = manifest.get("files", {}).get(relative)
            errors.require(isinstance(entry, dict), f"manifest: missing {relative}")
            if isinstance(entry, dict):
                errors.require(entry.get("records") == len(rows), f"manifest: wrong record count for {relative}")
                errors.require(entry.get("sha256") == sha256(language_dir / filename), f"manifest: wrong SHA-256 for {relative}")

    errors.require(set(manifest.get("files", {})) == {f"{language}/{filename}" for language in ("cn", "en") for filename in wanted}, "manifest: file list differs from generated CSV files")
    for filename in sorted(wanted):
        if filename not in language_data["cn"] or filename not in language_data["en"]:
            continue
        cn_columns, cn_rows, cn_keys = language_data["cn"][filename]
        en_columns, en_rows, en_keys = language_data["en"][filename]
        errors.require(cn_columns == en_columns, f"{filename}: cn/en columns differ")
        errors.require(cn_keys == en_keys, f"{filename}: cn/en ID or lookup key sets differ")
        errors.require([row.get("english") for row in cn_rows] == [row.get("english") for row in en_rows], f"{filename}: cn/en English columns differ")

    type_rows = language_data.get("en", {}).get("equipment_types.csv", ([], [], []))[1]
    types = {int(row["id"]): row["english"] for row in type_rows if row.get("id", "").isdecimal()}
    errors.require(set(types) == set(range(21)), "equipment_types.csv: IDs must cover exactly 0..20")
    for identifier, name in ((11, "Light Bowgun"), (12, "Heavy Bowgun"), (19, "Insect Glaive"), (20, "Charge Blade")):
        errors.require(types.get(identifier) == name, f"equipment_types.csv: save type {identifier} must be {name}")

    report: dict[str, object] = {
        "data": str(data_dir),
        "files": len(manifest.get("files", {})),
        "structural_errors_before_samples": len(errors.messages),
    }
    if samples is not None and samples.is_dir():
        report["real_save_coverage"] = validate_samples(data_dir, mapping, samples, errors)
    else:
        report["real_save_coverage"] = {"status": "skipped", "reason": "sample directory not found"}
    report["errors"] = errors.messages
    if report_path is not None:
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    return errors, report


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    default_samples = script_dir.parent.parent / "research" / "mh4g" / "samples"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data", type=Path, help="generated data directory")
    parser.add_argument("--mapping", type=Path, default=script_dir / "data_mapping.json")
    parser.add_argument("--samples", type=Path, default=default_samples)
    parser.add_argument("--no-samples", action="store_true", help="skip real-save coverage checks")
    parser.add_argument("--report", type=Path, help="optional JSON report path")
    args = parser.parse_args()
    sample_dir = None if args.no_samples else args.samples.resolve()
    errors, report = validate(args.data.resolve(), args.mapping.resolve(), sample_dir, args.report.resolve() if args.report else None)
    coverage = report.get("real_save_coverage", {})
    if isinstance(coverage, dict) and coverage.get("status") != "skipped":
        saves = coverage.get("saves", [])
        print(f"real-save coverage: {len(saves)} saves, {len(coverage.get('unmatched', []))} unmatched records")
        for row in saves:
            print(
                f"  {row['file']}: items={row['nonempty_item_slots']}, "
                f"equipment={row['nonempty_equipment_slots']}, "
                f"last100={row['nonempty_last_100_equipment_slots']}, "
                f"decorations={row['decoration_references']}"
            )
    if errors.messages:
        print(f"validation failed with {len(errors.messages)} error(s):")
        for message in errors.messages:
            print("  - " + message)
        return 1
    print(f"validation passed: {report.get('files', 0)} CSV files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
