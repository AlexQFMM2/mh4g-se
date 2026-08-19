#!/usr/bin/env python3
"""Build the deterministic MH4G runtime SQLite database.

The generated database deliberately uses the save IDs extracted from the game
code as its primary keys.  Dex rows only enrich those records; they never
assign an on-disk ID.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sqlite3
from pathlib import Path


FORMAT = "mh4g-save-editor-sqlite-v1"
GENERATOR_VERSION = "1.1.1"
ARMOR_FILES = {1: "armor_chest.csv", 2: "armor_arms.csv", 3: "armor_waist.csv", 4: "armor_legs.csv", 5: "armor_head.csv"}
RELIC_COLOR_SUFFIXES = {
    "red": "红",
    "yellow": "黄",
    "green": "绿",
    "blue": "蓝",
    "purple": "紫",
}


def rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        return list(csv.DictReader(handle))


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def integer(value: str | int | None, default: int = 0) -> int:
    try:
        return int(str(value).strip())
    except (TypeError, ValueError):
        return default


def normalized(value: str) -> str:
    value = re.sub(r"\s*\((red|yellow|green|blue|purple)\)\s*$", "", value, flags=re.I)
    value = value.replace("+", " plus ").replace("&", " and ").casefold()
    return re.sub(r"[^a-z0-9]+", "", value)


def schema(connection: sqlite3.Connection) -> None:
    connection.executescript(
        """
        PRAGMA foreign_keys=ON;
        PRAGMA user_version=1;
        CREATE TABLE meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);
        CREATE TABLE sources(name TEXT PRIMARY KEY,sha256 TEXT NOT NULL,detail TEXT NOT NULL);
        CREATE TABLE equipment_types(save_type INTEGER PRIMARY KEY,subtype TEXT NOT NULL,name_cn TEXT NOT NULL,name_en TEXT NOT NULL);
        CREATE TABLE items(save_id INTEGER PRIMARY KEY,name_cn TEXT NOT NULL,name_en TEXT NOT NULL,source TEXT NOT NULL);
        CREATE TABLE weapons(
          save_type INTEGER NOT NULL,save_id INTEGER NOT NULL,name_cn TEXT NOT NULL,name_en TEXT NOT NULL,
          rarity INTEGER NOT NULL,is_relic INTEGER NOT NULL CHECK(is_relic IN(0,1)),layout TEXT NOT NULL,
          attack INTEGER NOT NULL,defense INTEGER NOT NULL,affinity INTEGER NOT NULL,slots INTEGER NOT NULL,
          special1_id INTEGER NOT NULL,special1_value INTEGER NOT NULL,special2_id INTEGER NOT NULL,special2_value INTEGER NOT NULL,
          mapping_status TEXT NOT NULL,source TEXT NOT NULL,raw_hex TEXT NOT NULL,
          PRIMARY KEY(save_type,save_id));
        CREATE TABLE armors(
          save_type INTEGER NOT NULL,save_id INTEGER NOT NULL,name_cn TEXT NOT NULL,name_en TEXT NOT NULL,
          rarity INTEGER NOT NULL,is_relic INTEGER NOT NULL CHECK(is_relic IN(0,1)),model_flags INTEGER NOT NULL,special_flags INTEGER NOT NULL,
          combat INTEGER NOT NULL,gender INTEGER NOT NULL,slots INTEGER NOT NULL,base_defense INTEGER NOT NULL,max_defense INTEGER,
          fire_res INTEGER NOT NULL,water_res INTEGER NOT NULL,thunder_res INTEGER NOT NULL,ice_res INTEGER NOT NULL,dragon_res INTEGER NOT NULL,
          mapping_status TEXT NOT NULL,source TEXT NOT NULL,raw_hex TEXT NOT NULL,
          PRIMARY KEY(save_type,save_id));
        CREATE TABLE armor_skill_points(
          save_type INTEGER NOT NULL,save_id INTEGER NOT NULL,skill_tree_id INTEGER NOT NULL,points INTEGER NOT NULL,
          PRIMARY KEY(save_type,save_id,skill_tree_id),FOREIGN KEY(save_type,save_id) REFERENCES armors(save_type,save_id));
        CREATE TABLE decorations(
          save_id INTEGER PRIMARY KEY,item_save_id INTEGER NOT NULL,name_cn TEXT NOT NULL,name_en TEXT NOT NULL,
          slots INTEGER NOT NULL,flags INTEGER NOT NULL,source TEXT NOT NULL,raw_hex TEXT NOT NULL);
        CREATE TABLE decoration_skill_points(
          decoration_id INTEGER NOT NULL REFERENCES decorations(save_id),skill_tree_id INTEGER NOT NULL,points INTEGER NOT NULL,
          PRIMARY KEY(decoration_id,skill_tree_id));
        CREATE TABLE skill_trees(id INTEGER PRIMARY KEY,name_cn TEXT NOT NULL,name_en TEXT NOT NULL,source TEXT NOT NULL);
        CREATE TABLE active_skills(id INTEGER PRIMARY KEY,skill_tree_id INTEGER NOT NULL REFERENCES skill_trees(id),points INTEGER NOT NULL,name_cn TEXT NOT NULL,name_en TEXT NOT NULL);
        CREATE TABLE charm_classes(save_id INTEGER PRIMARY KEY,name_cn TEXT NOT NULL,name_en TEXT NOT NULL,source TEXT NOT NULL);
        CREATE TABLE equipment_lookups(
          domain TEXT NOT NULL,save_type INTEGER NOT NULL,variant TEXT NOT NULL,code INTEGER NOT NULL,
          name_cn TEXT NOT NULL,name_en TEXT NOT NULL,source TEXT NOT NULL,PRIMARY KEY(domain,save_type,variant,code));
        CREATE TABLE relic_weapon_attack_values(
          save_type INTEGER NOT NULL,code INTEGER NOT NULL,true_attack INTEGER NOT NULL,affinity INTEGER NOT NULL,defense INTEGER NOT NULL,
          source TEXT NOT NULL,PRIMARY KEY(save_type,code));
        CREATE TABLE relic_armor_defense_values(code INTEGER PRIMARY KEY,defense INTEGER NOT NULL,source TEXT NOT NULL);
        CREATE TABLE relic_armor_resistance_values(
          code INTEGER PRIMARY KEY,fire_res INTEGER NOT NULL,water_res INTEGER NOT NULL,thunder_res INTEGER NOT NULL,ice_res INTEGER NOT NULL,dragon_res INTEGER NOT NULL,source TEXT NOT NULL);
        CREATE INDEX idx_weapons_relic ON weapons(is_relic,save_type,save_id);
        CREATE INDEX idx_armors_relic ON armors(is_relic,save_type,save_id);
        CREATE INDEX idx_armor_skills ON armor_skill_points(skill_tree_id,points,save_type,save_id);
        CREATE INDEX idx_decoration_skills ON decoration_skill_points(skill_tree_id,points,decoration_id);
        CREATE INDEX idx_active_skills ON active_skills(skill_tree_id,points,id);
        """
    )


def dex_armor_maxima(dex: Path) -> dict[tuple[int, str], tuple[int, int, int]]:
    if not dex.exists():
        return {}
    data = {integer(row["Amr_ID"]): row for row in rows(dex / "DB_Amr.csv")}
    names = {integer(row["Amr_ID"]): row for row in rows(dex / "ID_Amr_Name.csv")}
    part_to_type = {1: 5, 2: 1, 3: 2, 4: 3, 5: 4}
    result: dict[tuple[int, str], tuple[int, int, int]] = {}
    for identifier, row in data.items():
        name = names.get(identifier, {}).get("Amr_Name_0", "")
        save_type = part_to_type.get(integer(row.get("Part")))
        if save_type and name:
            result[(save_type, normalized(name))] = (integer(row.get("MaxDef"), -1), integer(row.get("BorG")), integer(row.get("MorF")))
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv-root", type=Path, default=Path("data"))
    parser.add_argument("--native-dir", type=Path, required=True)
    parser.add_argument("--dex-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("data/mh4g.sqlite"))
    parser.add_argument("--manifest", type=Path, default=Path("data/manifest.json"))
    args = parser.parse_args()

    cn, en = args.csv_root / "cn", args.csv_root / "en"
    native = args.native_dir
    source_entries = sorted(
        [(f"csv/cn/{path.name}", path) for path in cn.glob("*.csv")]
        + [(f"csv/en/{path.name}", path) for path in en.glob("*.csv")]
        + [(f"native/{path.name}", path) for path in native.glob("*.csv")]
        + ([("native/manifest.json", native / "manifest.json")] if (native / "manifest.json").is_file() else [])
        + [(f"dex-build7/{path.name}", path) for path in args.dex_dir.glob("*.csv")],
        key=lambda entry: entry[0],
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(".sqlite.tmp")
    temporary.unlink(missing_ok=True)
    connection = sqlite3.connect(temporary)
    schema(connection)
    connection.execute("INSERT INTO meta VALUES('format',?)", (FORMAT,))
    connection.execute("INSERT INTO meta VALUES('generator_version',?)", (GENERATOR_VERSION,))

    for name, path in source_entries:
        connection.execute("INSERT INTO sources VALUES(?,?,?)", (name, digest(path), "generated input"))

    type_cn = {integer(r["id"]): r for r in rows(cn / "equipment_types.csv")}
    type_en = {integer(r["id"]): r for r in rows(en / "equipment_types.csv")}
    for save_type in sorted(type_cn):
        subtype = "none" if save_type == 0 else "armor" if 1 <= save_type <= 5 else "charm" if save_type == 6 else "weapon"
        connection.execute("INSERT INTO equipment_types VALUES(?,?,?,?)", (save_type, subtype, type_cn[save_type]["name"], type_en[save_type]["name"]))

    for file_name, table in (("items.csv", "items"),):
        cn_rows = {integer(r["id"]): r for r in rows(cn / file_name)}
        en_rows = {integer(r["id"]): r for r in rows(en / file_name)}
        for identifier in sorted(cn_rows):
            connection.execute(f"INSERT INTO {table} VALUES(?,?,?,?)", (identifier, cn_rows[identifier]["name"], en_rows[identifier]["name"], cn_rows[identifier]["source"]))

    skill_cn = {integer(r["id"]): r for r in rows(cn / "skills.csv")}
    skill_en = {integer(r["id"]): r for r in rows(en / "skills.csv")}
    for identifier in sorted(skill_cn):
        connection.execute("INSERT INTO skill_trees VALUES(?,?,?,?)", (identifier, skill_cn[identifier]["name"], skill_en[identifier]["name"], skill_cn[identifier]["source"]))

    active_names_cn = {integer(r["Skl_ID"]): r for r in rows(args.dex_dir / "ID_Skl_Name.csv")}
    for row in rows(args.dex_dir / "DB_Skl.csv"):
        identifier, tree_id, points = integer(row["Skl_ID"]), integer(row["SklTree_ID"]), integer(row["Pt"])
        if tree_id not in skill_cn or identifier not in active_names_cn:
            continue
        name = active_names_cn[identifier]
        connection.execute("INSERT INTO active_skills VALUES(?,?,?,?,?)", (identifier, tree_id, points, name.get("Skl_Name_1") or name.get("Skl_Name_0") or "", name.get("Skl_Name_0") or ""))

    maxima = dex_armor_maxima(args.dex_dir)
    for save_type, file_name in ARMOR_FILES.items():
        names_cn = {integer(r["id"]): r for r in rows(cn / file_name)}
        names_en = {integer(r["id"]): r for r in rows(en / file_name)}
        for row in rows(native / file_name):
            identifier = integer(row["save_id"])
            cn_row, en_row = names_cn[identifier], names_en[identifier]
            max_defense, combat, gender = maxima.get((save_type, normalized(en_row["name"])), (None, 0, 0))
            is_relic = 1 if integer(row["special_flags"]) & 0x80 else 0
            status = "confirmed" if max_defense is not None or is_relic else "native_only"
            connection.execute(
                "INSERT INTO armors VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (save_type, identifier, cn_row["name"], en_row["name"], integer(row["rarity"]), is_relic,
                 integer(row["model_flags"]), integer(row["special_flags"]), combat, gender, integer(row["slots"]),
                 integer(row["base_defense"]), max_defense, integer(row["fire_resistance"]), integer(row["water_resistance"]),
                 integer(row["thunder_resistance"]), integer(row["ice_resistance"]), integer(row["dragon_resistance"]),
                 status, cn_row["source"] + "+mh4g-code-bin", row["raw_hex"]),
            )
            for index in range(1, 6):
                skill_id, points = integer(row[f"skill_{index}_id"]), integer(row[f"skill_{index}_points"])
                if skill_id and points:
                    connection.execute("INSERT OR REPLACE INTO armor_skill_points VALUES(?,?,?,?)", (save_type, identifier, skill_id, points))

    weapon_names: dict[int, tuple[dict[int, dict[str, str]], dict[int, dict[str, str]]]] = {}
    for spec in json.loads((Path("tools/data_mapping.json")).read_text(encoding="utf-8"))["equipment"]:
        if spec["kind"] == "weapon":
            weapon_names[spec["save_type"]] = (
                {integer(r["id"]): r for r in rows(cn / spec["file"])},
                {integer(r["id"]): r for r in rows(en / spec["file"])},
            )
    for row in rows(native / "weapons.csv"):
        save_type, identifier = integer(row["save_type"]), integer(row["save_id"])
        cn_rows, en_rows = weapon_names[save_type]
        cn_row, en_row = cn_rows[identifier], en_rows[identifier]
        is_relic = integer(row["is_relic"])
        if is_relic:
            match = re.search(r"\((red|yellow|green|blue|purple)\)\s*$", en_row["name"], re.I)
            expected_suffix = f"（发掘·{RELIC_COLOR_SUFFIXES[match.group(1).lower()]}）" if match else ""
            if not expected_suffix or not cn_row["name"].endswith(expected_suffix):
                raise ValueError(
                    f"relic weapon {save_type}:{identifier} is missing its reviewed colour suffix: "
                    f"cn={cn_row['name']!r}, en={en_row['name']!r}"
                )
        connection.execute(
            "INSERT INTO weapons VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            (save_type, identifier, cn_row["name"], en_row["name"], integer(row["rarity"]), is_relic, row["layout"],
             integer(row["attack_raw"]), integer(row["defense"]), integer(row["affinity_percent"]), integer(row["slots"]),
             integer(row["special_1_id"]), integer(row["special_1_value"]), integer(row["special_2_id"]), integer(row["special_2_value"]),
             "confirmed", cn_row["source"] + "+mh4g-code-bin", row["raw_hex"]),
        )

    decoration_cn = {integer(r["id"]): r for r in rows(cn / "decorations.csv")}
    decoration_en = {integer(r["id"]): r for r in rows(en / "decorations.csv")}
    for row in rows(native / "decorations.csv"):
        identifier = integer(row["save_id"])
        connection.execute("INSERT INTO decorations VALUES(?,?,?,?,?,?,?,?)", (identifier, integer(row["item_save_id"]), decoration_cn[identifier]["name"], decoration_en[identifier]["name"], integer(row["slots"]), integer(row["flags"]), decoration_cn[identifier]["source"] + "+mh4g-code-bin", row["raw_hex"]))
        for index in range(1, 3):
            skill_id, points = integer(row[f"skill_{index}_id"]), integer(row[f"skill_{index}_points"])
            if skill_id and points:
                connection.execute("INSERT INTO decoration_skill_points VALUES(?,?,?)", (identifier, skill_id, points))

    charm_cn = {integer(r["id"]): r for r in rows(cn / "talismans.csv")}
    charm_en = {integer(r["id"]): r for r in rows(en / "talismans.csv")}
    for identifier in sorted(charm_cn):
        connection.execute("INSERT INTO charm_classes VALUES(?,?,?,?)", (identifier, charm_cn[identifier]["name"], charm_en[identifier]["name"], charm_cn[identifier]["source"]))

    lookup_cn = rows(cn / "equipment_lookups.csv")
    lookup_en = {(r["domain"], integer(r["equipment_type"]), r["variant"], integer(r["value"])): r for r in rows(en / "equipment_lookups.csv")}
    resistance_pattern = re.compile(r"Fi\s*(-?\d+)\s*/\s*Wa\s*(-?\d+)\s*/\s*Th\s*(-?\d+)\s*/\s*Ic\s*(-?\d+)\s*/\s*Dr\s*(-?\d+)")
    attack_pattern = re.compile(r"^\s*(-?\d+)\s*\|\s*([^|]*)\|\s*(.*)$")
    for row in lookup_cn:
        key = (row["domain"], integer(row["equipment_type"]), row["variant"], integer(row["value"]))
        connection.execute("INSERT INTO equipment_lookups VALUES(?,?,?,?,?,?,?)", (*key, row["name"], lookup_en[key]["name"], row["source"]))
        if row["domain"] == "relic_defense" and key[1] == 1:
            connection.execute("INSERT OR IGNORE INTO relic_armor_defense_values VALUES(?,?,?)", (key[3], integer(row["name"], -1), row["source"]))
        elif row["domain"] == "relic_resistance" and key[1] == 1:
            match = resistance_pattern.search(row["name"].replace("/Wa", "/ Wa"))
            if match:
                connection.execute("INSERT OR IGNORE INTO relic_armor_resistance_values VALUES(?,?,?,?,?,?,?)", (key[3], *map(int, match.groups()), row["source"]))
        elif row["domain"] == "attack_tier":
            match = attack_pattern.match(row["name"])
            if match:
                affinity_match = re.search(r"([+-]?\d+)%", match.group(2))
                defense_match = re.search(r"DEF\s*([+-]?\d+)", match.group(3), re.I)
                connection.execute("INSERT OR IGNORE INTO relic_weapon_attack_values VALUES(?,?,?,?,?,?)", (key[1], key[3], int(match.group(1)), int(affinity_match.group(1)) if affinity_match else 0, int(defense_match.group(1)) if defense_match else 0, row["source"]))

    connection.execute("INSERT INTO meta VALUES('armor_count',(SELECT count(*) FROM armors))")
    connection.execute("INSERT INTO meta VALUES('weapon_count',(SELECT count(*) FROM weapons))")
    connection.execute("INSERT INTO meta VALUES('decoration_count',(SELECT count(*) FROM decorations))")
    connection.commit()
    connection.execute("VACUUM")
    connection.close()
    temporary.replace(args.output)

    native_manifest = json.loads((native / "manifest.json").read_text(encoding="utf-8")) if (native / "manifest.json").is_file() else {}
    manifest = {
        "format": "mh4g-save-editor-data-manifest-v2",
        "generator": {"name": Path(__file__).name, "version": GENERATOR_VERSION},
        "database": {"file": args.output.name, "sha256": digest(args.output), "bytes": args.output.stat().st_size},
        "counts": {"armors": 4835, "weapons": 2849, "decorations": 290},
        "native_export": {"format": native_manifest.get("format", ""), "code_sha256": native_manifest.get("code_sha256", "")},
        "sources": [{"name": name, "sha256": digest(path)} for name, path in source_entries],
    }
    args.manifest.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest["database"], ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
