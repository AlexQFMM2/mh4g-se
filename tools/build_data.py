#!/usr/bin/env python3
"""Build the deterministic MH4G save-editor CSV dataset."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import shutil
import tempfile
import unicodedata
from collections import defaultdict
from pathlib import Path
from typing import Iterable


GENERATOR_VERSION = "2.0.0"
GAME_EXPORT_FORMAT = "mh4g-game-name-export-v1"
GAME_RESOURCE_SOURCE = "mh4g-core-common-game-array"
DEX_SOURCE = "mh4g-dex-build7"
DEX_FALLBACK_SOURCE = "mh4g-dex-build7-en-fallback"
REFERENCE_SOURCE = "mh4edit-mit-save-id"
REFERENCE_FALLBACK_SOURCE = "mh4edit-mit-save-id-en-fallback"
SAVE_ID_CROSSWALK_SOURCE = "mh4g-save-id-dex-crosswalk"
ARMOR_CROSSWALK_SOURCE = "mh4g-armor-name-crosswalk"
TALISMAN_CROSSWALK_SOURCE = "mh4g-talisman-name-crosswalk"
SAVE_FORMAT_SOURCE = "mh4g-save-format"
MH4U_SHARPNESS_SOURCE = "mikewii-mh4u-editor-sharpness"
KINSECT_CN_SOURCE = "mh4g-kinsect-effect-cn-crosswalk"

BASE_COLUMNS = ("id", "name", "english", "source")
EQUIPMENT_COLUMNS = BASE_COLUMNS + ("rarity", "is_relic")
LOOKUP_COLUMNS = ("domain", "equipment_type", "variant", "value", "name", "english", "source")

EQUIPMENT_TYPES = (
    (0, "无", "None"),
    (1, "胴甲", "Chest"),
    (2, "腕甲", "Arms"),
    (3, "腰甲", "Waist"),
    (4, "腿甲", "Legs"),
    (5, "头甲", "Head"),
    (6, "护石", "Talisman"),
    (7, "大剑", "Great Sword"),
    (8, "片手剑", "Sword and Shield"),
    (9, "锤", "Hammer"),
    (10, "长枪", "Lance"),
    (11, "轻弩", "Light Bowgun"),
    (12, "重弩", "Heavy Bowgun"),
    (13, "太刀", "Long Sword"),
    (14, "斩击斧", "Switch Axe"),
    (15, "铳枪", "Gunlance"),
    (16, "弓", "Bow"),
    (17, "双剑", "Dual Blades"),
    (18, "狩猎笛", "Hunting Horn"),
    (19, "操虫棍", "Insect Glaive"),
    (20, "盾斧", "Charge Blade"),
)

COLOR_SUFFIXES = {
    "red": "红",
    "yellow": "黄",
    "green": "绿",
    "blue": "蓝",
    "purple": "紫",
}

KINSECT_TYPE_CN = {
    0x00: "切断 · 无加成",
    0x01: "切断 · 全能力小",
    0x02: "切断 · 攻击小",
    0x03: "切断 · 攻击大",
    0x04: "切断 · 攻击中 / 回复精华效果小",
    0x05: "切断 · 耐力小",
    0x06: "切断 · 耐力大",
    0x07: "切断 · 耐力中 / 回复精华效果小",
    0x08: "切断 · 速度小",
    0x09: "切断 · 速度大",
    0x0A: "切断 · 速度中 / 回复精华效果小",
    0x0B: "切断 · 全能力中 / 回复精华效果大",
    0x0C: "打击 · 无加成",
    0x0D: "打击 · 全能力小",
    0x0E: "打击 · 攻击小",
    0x0F: "打击 · 攻击大",
    0x10: "打击 · 攻击中 / 回复精华效果小",
    0x11: "打击 · 耐力小",
    0x12: "打击 · 耐力大",
    0x13: "打击 · 耐力中 / 回复精华效果小",
    0x14: "打击 · 速度小",
    0x15: "打击 · 速度大",
    0x16: "打击 · 速度中 / 回复精华效果小",
    0x17: "打击 · 全能力中 / 回复精华效果大",
    0x18: "打击 · 无加成（未使用）",
    0x19: "切断 · 攻击+耐力大 / 精华效果小 / 时间延长",
    0x1A: "切断 · 耐力+速度大 / 精华效果小 / 蓄力缩短",
    0x1B: "切断 · 攻击+速度大 / 精华效果小 / 贯通强化",
    0x1C: "切断 · 全能力大 / 精华+状态强化 / 精华+1",
    0x1D: "打击 · 攻击+耐力大 / 精华效果小 / 时间延长",
    0x1E: "打击 · 耐力+速度大 / 精华效果小 / 蓄力缩短",
    0x1F: "打击 · 攻击+速度大 / 精华效果小 / 贯通强化",
    0x20: "打击 · 全能力大 / 精华+状态强化 / 精华+1",
    0x21: "打击 · 速度小 / 精华时间延长",
    0x22: "切断 · 精华+1（未使用）",
}


def read_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        return list(csv.DictReader(handle))


def write_csv(path: Path, columns: Iterable[str], rows: Iterable[dict[str, object]]) -> int:
    materialized = list(rows)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(columns), lineterminator="\n")
        writer.writeheader()
        writer.writerows(materialized)
    return len(materialized)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def normalized_name(value: str) -> str:
    # Preserve punctuation with semantic meaning before stripping typography.
    value = value.replace("+", " plus ").replace("&", " and ")
    ascii_value = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"[^a-z0-9]+", "", ascii_value.casefold())


def decimal(value: str | int) -> int:
    return int(str(value).strip(), 10)


def hexadecimal(value: str) -> int:
    return int(value.strip(), 16)


def source_name(base: str, fallback: bool) -> str:
    if base == DEX_SOURCE:
        return DEX_FALLBACK_SOURCE if fallback else DEX_SOURCE
    return base


def localized_name(row: dict[str, str], prefix: str, language_index: int) -> tuple[str, str, bool]:
    english = row[f"{prefix}0"].strip()
    localized = row.get(f"{prefix}{language_index}", "").strip()
    fallback = language_index != 0 and (not localized or localized == "---")
    if language_index == 0 or fallback:
        localized = english
    return localized, english, fallback


def joined_rows(
    data_rows: list[dict[str, str]],
    name_rows: list[dict[str, str]],
    id_column: str,
) -> list[dict[str, str]]:
    names = {decimal(row[id_column]): row for row in name_rows}
    result = []
    for row in data_rows:
        identifier = decimal(row[id_column])
        if identifier not in names:
            raise ValueError(f"{id_column} {identifier} has no name row")
        result.append({**row, **names[identifier]})
    return result


def make_items(
    sql_dir: Path,
    mapping: dict,
    game_names: list[str],
    language: str,
) -> tuple[list[dict[str, object]], int]:
    spec = mapping["tables"]["items"]
    rows = joined_rows(
        read_csv(sql_dir / spec["data"]),
        read_csv(sql_dir / spec["names"]),
        spec["id"],
    )
    by_save_id = {hexadecimal(row[spec["save_id"]]): row for row in rows}
    missing_english = {
        0: "None",
        124: "No Bottle",
        1924: "10000z",
        1925: "1000 Caravan Points",
        1936: "No Item",
    }
    result = []
    missing = []
    for identifier, game_name in enumerate(game_names):
        row = by_save_id.get(identifier)
        if row is not None:
            english = row[f"{spec['name_prefix']}0"].strip()
            source = GAME_RESOURCE_SOURCE + "+" + DEX_SOURCE
        elif game_name.casefold().startswith("dummy"):
            english = game_name.upper()
            source = GAME_RESOURCE_SOURCE
        elif identifier in missing_english:
            english = missing_english[identifier]
            source = GAME_RESOURCE_SOURCE
        else:
            missing.append((identifier, game_name))
            continue
        result.append({
            "id": identifier,
            "name": game_name if language == "cn" else english,
            "english": english,
            "source": source,
        })
    if missing:
        raise ValueError(f"game item IDs without an explicit Dex or placeholder mapping: {missing[:10]}")
    return result, len(game_names) - len(rows)


def make_skills(sql_dir: Path, mapping: dict, language: str, language_index: int) -> list[dict[str, object]]:
    spec = mapping["tables"]["skills"]
    rows = read_csv(sql_dir / spec["names"])
    result = [{"id": 0, "name": "无" if language == "cn" else "None", "english": "None", "source": SAVE_FORMAT_SOURCE}]
    for row in rows:
        identifier = decimal(row[spec["id"]])
        if identifier <= 0:
            continue
        name, english, fallback = localized_name(row, spec["name_prefix"], language_index)
        result.append({"id": identifier, "name": name, "english": english, "source": source_name(DEX_SOURCE, fallback)})
    return sorted(result, key=lambda row: row["id"])


def index_item_names(sql_dir: Path, mapping: dict) -> dict[str, list[dict[str, str]]]:
    spec = mapping["tables"]["items"]
    result: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in read_csv(sql_dir / spec["names"]):
        result[normalized_name(row[f"{spec['name_prefix']}0"])].append(row)
    return result


def make_decorations(
    sql_dir: Path,
    mapping: dict,
    reference: dict,
    language: str,
    language_index: int,
) -> tuple[list[dict[str, object]], int]:
    spec = mapping["tables"]["decorations"]
    names_by_key = index_item_names(sql_dir, mapping)
    jewel_items = {decimal(row[spec["item_id"]]) for row in read_csv(sql_dir / spec["data"])}
    item_id_column = mapping["tables"]["items"]["id"]
    name_prefix = mapping["tables"]["items"]["name_prefix"]
    reference_names = reference["arrays"][spec["reference_array"]]
    result = []
    unmatched = 0
    for identifier, reference_english in enumerate(reference_names):
        if identifier == 0:
            result.append({"id": 0, "name": "无" if language == "cn" else "None", "english": "None", "source": SAVE_FORMAT_SOURCE})
            continue
        candidates = [
            row for row in names_by_key.get(normalized_name(reference_english), [])
            if decimal(row[item_id_column]) in jewel_items
        ]
        if candidates:
            candidates.sort(key=lambda row: decimal(row[item_id_column]))
            name, english, fallback = localized_name(candidates[0], name_prefix, language_index)
            result.append({"id": identifier, "name": name, "english": english, "source": source_name(DEX_SOURCE, fallback) + "+" + REFERENCE_SOURCE})
        else:
            unmatched += 1
            result.append({
                "id": identifier,
                "name": reference_english,
                "english": reference_english,
                "source": REFERENCE_FALLBACK_SOURCE if language == "cn" else REFERENCE_SOURCE,
            })
    return result, unmatched


def equipment_candidates(
    sql_dir: Path, mapping: dict, spec: dict
) -> tuple[dict[str, list[dict[str, str]]], dict[int, dict[str, str]]]:
    table_spec = mapping["tables"][spec["kind"] + ("s" if spec["kind"] == "weapon" else "")]
    rows = joined_rows(
        read_csv(sql_dir / table_spec["data"]),
        read_csv(sql_dir / table_spec["names"]),
        table_spec["id"],
    )
    selector = table_spec["type"] if spec["kind"] == "weapon" else table_spec["part"]
    filtered = [row for row in rows if decimal(row[selector]) == spec["dex_value"]]
    result: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in filtered:
        result[normalized_name(row[f"{table_spec['name_prefix']}0"])].append(row)
    for candidates in result.values():
        candidates.sort(key=lambda row: decimal(row[table_spec["id"]]))
    by_id = {decimal(row[table_spec["id"]]): row for row in filtered}
    return result, by_id


def split_relic_name(name: str) -> tuple[str, str | None]:
    match = re.fullmatch(r"(.+?)\s*\((red|yellow|green|blue|purple)\)", name, re.IGNORECASE)
    return (match.group(1), match.group(2).lower()) if match else (name, None)


def armor_crosswalk_name(reference_english: str, crosswalk: dict) -> str | None:
    """Resolve a relic-armor display name through reviewed, explicit rules."""
    for suffix in sorted(crosswalk["suffixes"], key=len, reverse=True):
        match = re.fullmatch(
            rf"(.*?)\s*{re.escape(suffix)}(?:\s+([A-Z]))?",
            reference_english,
            re.IGNORECASE,
        )
        if not match:
            continue
        series_key = match.group(1).strip()
        if match.group(2):
            series_key += " " + match.group(2).upper()
        series = crosswalk["series"].get(series_key)
        if series is None:
            return None
        return series + crosswalk["suffixes"][suffix] + "（发掘）"
    return None


def authoritative_reference_names(spec: dict, reference: dict) -> list[str]:
    """Return English metadata labels aligned to the authoritative game IDs.

    Every legacy reference array except waist already has the exact number of
    slots found in core_common.arc. The legacy waist array omitted eleven
    early entries, mislabeled six real DLC entries as placeholders, and ended
    with eight non-waist rows. Reconstruct that crosswalk explicitly; IDs and
    Chinese display names still always come from the game resource.
    """
    legacy = reference["arrays"][spec["reference_array"]]
    if spec["file"] != "armor_waist.csv":
        return legacy
    corrected = [
        "None",
        "Derring Belt", "Derring Faulds S", "Derring Coat S",
        "Leather Belt", "Leather Faulds S", "Leather Coat S",
        "Chainmail Belt", "Chainmail Faulds S", "Chainmail Coat S",
        legacy[3], legacy[4], legacy[1], legacy[2], legacy[3], legacy[4],
        *legacy[5:837],
        "[JP] Treason King Coil J", "[JP] Treason King Skirt J",
        "[JP] Champion's Belt", "[JP] Champion's Belt",
        "[JP] Strongman's Belt", "[JP] Strongman's Belt",
        *("DUMMY" for _ in range(6)),
        *legacy[849:945],
    ]
    if len(corrected) != spec["game_count"]:
        raise ValueError(f"corrected waist crosswalk has {len(corrected)} rows")
    return corrected


def make_equipment(
    sql_dir: Path,
    mapping: dict,
    reference: dict,
    weapon_crosswalk: dict,
    armor_crosswalk: dict,
    game_names: list[str],
    spec: dict,
    language: str,
    language_index: int,
) -> tuple[list[dict[str, object]], dict[str, int]]:
    table_spec = mapping["tables"][spec["kind"] + ("s" if spec["kind"] == "weapon" else "")]
    candidates, candidates_by_id = equipment_candidates(sql_dir, mapping, spec)
    reference_names = authoritative_reference_names(spec, reference)
    if len(reference_names) != len(game_names):
        raise ValueError(
            f"{spec['file']}: English metadata has {len(reference_names)} rows, "
            f"game resource has {len(game_names)}"
        )
    result = []
    matched = 0
    relics = 0
    save_id_dex_matches = 0
    relic_cn_matches = 0
    relic_id_spec = weapon_crosswalk["relic_save_ids"].get(str(spec["save_type"]))
    relic_ids = set(equipment_type_values(relic_id_spec)) if relic_id_spec else set()
    for identifier, (game_name, reference_english) in enumerate(zip(game_names, reference_names)):
        if identifier == 0:
            result.append({
                "id": 0,
                "name": game_name if language == "cn" else "None",
                "english": "None",
                "source": GAME_RESOURCE_SOURCE,
                "rarity": 0,
                "is_relic": 0,
            })
            continue
        is_relic = identifier in relic_ids
        base_english, color = split_relic_name(reference_english) if is_relic else (reference_english, None)
        if is_relic and color is None:
            raise ValueError(f"relic weapon {spec['save_type']}:{identifier} has no color suffix")
        relics += int(is_relic)
        choices = candidates.get(normalized_name(base_english), [])
        save_key = f"{spec['save_type']}:{identifier}"
        dex_id = weapon_crosswalk["ordinary_dex_ids"].get(save_key)
        if not is_relic and dex_id is not None:
            if dex_id not in candidates_by_id:
                raise ValueError(f"weapon crosswalk {save_key} points outside its Dex weapon type: {dex_id}")
            choices = [candidates_by_id[dex_id]]
            save_id_dex_matches += 1
        if choices:
            matched += 1
            row = choices[0]
            name, dex_english, fallback = localized_name(row, table_spec["name_prefix"], language_index)
            if is_relic:
                english = reference_english
                name = f"{name}（发掘·{COLOR_SUFFIXES[color]}）" if language == "cn" else english
                rarity = 0
            else:
                english = dex_english
                if language == "en":
                    name = english
                rarity = decimal(row[table_spec["rarity"]])
            source = GAME_RESOURCE_SOURCE + "+" + DEX_SOURCE + "+" + REFERENCE_SOURCE
            if dex_id is not None:
                source += "+" + SAVE_ID_CROSSWALK_SOURCE
        else:
            relic_name = weapon_crosswalk["relic_base_names"].get(base_english) if is_relic else None
            armor_name = armor_crosswalk_name(reference_english, armor_crosswalk) if spec["kind"] == "armor" else None
            if relic_name is not None and language == "cn":
                name = game_name
                english = reference_english
                rarity = 0
                source = GAME_RESOURCE_SOURCE + "+" + relic_name["source"] + "+" + REFERENCE_SOURCE
                relic_cn_matches += 1
            elif armor_name is not None:
                name = game_name if language == "cn" else reference_english
                english = reference_english
                rarity = 0
                is_relic = True
                relics += 1
                source = GAME_RESOURCE_SOURCE + "+" + ARMOR_CROSSWALK_SOURCE + "+" + REFERENCE_SOURCE
                if language == "cn":
                    relic_cn_matches += 1
            else:
                name = game_name if language == "cn" else reference_english
                english = reference_english
                rarity = 0
                source = GAME_RESOURCE_SOURCE + "+" + REFERENCE_SOURCE
        if language == "cn":
            # The resource array is the authority for both the numeric ID and
            # the exact Chinese label, including DUMMY and DLC entries.
            name = game_name
        result.append({
            "id": identifier,
            "name": name,
            "english": english,
            "source": source,
            "rarity": rarity,
            "is_relic": int(is_relic),
        })
    return result, {
        "rows": len(result),
        "dex_name_matches": matched,
        "save_id_dex_matches": save_id_dex_matches,
        "relic_cn_matches": relic_cn_matches,
        "relic_rows": relics,
        "unmatched": len(result) - 1 - matched - relic_cn_matches,
    }


def make_talismans(reference: dict, language: str) -> list[dict[str, object]]:
    chinese_names = {
        1: "士兵护石",
        2: "斗士护石",
        3: "骑士护石",
        4: "城塞护石",
        5: "女王护石",
        6: "国王护石",
        7: "龙之护石",
        8: "未知护石",
        9: "神秘护石",
        10: "英雄护石",
        11: "传说护石",
        12: "天之护石",
        13: "贤者护石",
        14: "奇迹护石",
    }
    result = []
    for identifier, english in enumerate(reference["arrays"]["allEqpTalisman"]):
        name = ("无" if identifier == 0 else chinese_names[identifier]) if language == "cn" else ("None" if identifier == 0 else english)
        result.append({
            "id": identifier,
            "name": name,
            "english": "None" if identifier == 0 else english,
            "source": SAVE_FORMAT_SOURCE if identifier == 0 else (TALISMAN_CROSSWALK_SOURCE if language == "cn" else REFERENCE_SOURCE),
            "rarity": 0,
            "is_relic": 0,
        })
    return result


def equipment_type_values(value: str | list[int]) -> list[int]:
    if isinstance(value, list):
        return value
    result: list[int] = []
    for token in value.split(","):
        token = token.strip()
        if "-" in token:
            start, end = (int(part) for part in token.split("-", 1))
            result.extend(range(start, end + 1))
        else:
            result.append(int(token))
    return result


REFERENCE_LOOKUPS = (
    ("PolishReqValues", "polish_requirement", list(range(1, 6)) + list(range(7, 21)), "all"),
    ("Skill1AmountValues", "skill_points", [6], "primary"),
    ("Skill2AmountValues", "skill_points", [6], "secondary"),
    ("UpgradeArmorValues", "upgrade", list(range(1, 6)), "armor"),
    ("DefenseValues", "relic_defense", list(range(1, 6)), "armor"),
    ("RarityValues", "rarity", list(range(1, 6)) + list(range(7, 21)), "relic"),
    ("HoningValues", "honing", list(range(7, 21)), "weapon"),
    ("NumSlotsValues", "slots", list(range(1, 21)), "all"),
    ("SpecialValuesChargeblade", "weapon_special", [20], "relic_phial"),
    ("SpecialValuesSwitchaxe", "weapon_special", [14], "relic_phial"),
    ("SpecialValuesSnsLance", "weapon_special", [8, 10], "defense_boost"),
    ("SpecialValuesGreatswordHammer", "weapon_special", [7, 9], "attack_boost"),
    ("SpecialValuesLongswordDualblades", "weapon_special", [13, 17], "affinity_boost"),
    ("SpecialValuesGunlance", "weapon_special", [15], "shelling"),
    ("SpecialValuesInsectglaive", "kinsect", [19], "type"),
    ("SpecialValuesHuntinghorn", "weapon_special", [18], "notes"),
    ("SpecialValuesLbg", "weapon_special", [11], "available_shots"),
    ("SpecialValuesHbg", "weapon_special", [12], "available_shots"),
    ("SpecialValuesBow", "weapon_special", [16], "reserved"),
    ("UpgradeValues", "upgrade", list(range(7, 21)), "weapon"),
    ("ModifierValuesMelee", "attack_tier", [7, 8, 9, 10, 13, 14, 15, 17, 18, 19, 20], "melee"),
    ("ModifierValuesRanged", "attack_tier", [11, 12, 16], "ranged"),
    ("SharpnessValuesMelee", "sharpness", [7, 8, 9, 10, 13, 14, 15, 17, 18, 19, 20], "melee"),
    ("SharpnessValuesBow", "sharpness", [16], "bow"),
    ("SharpnessValuesLbg", "sharpness", [11], "light_bowgun"),
    ("SharpnessValuesHbg", "sharpness", [12], "heavy_bowgun"),
    ("ElementValuesLow", "attribute_value", [8, 16, 17, 19], "low"),
    ("ElementValuesMedium", "attribute_value", [9, 10, 13, 14, 15, 20], "medium"),
    ("ElementValuesHigh", "attribute_value", [7, 18], "high"),
    ("StatusValuesLow", "status_value", [8, 16, 17, 19], "low"),
    ("StatusValuesMedium", "status_value", [9, 10, 13, 14, 15, 20], "medium"),
    ("StatusValuesHigh", "status_value", [7, 18], "high"),
    ("ElementTypes", "attribute_type", list(range(7, 21)), "relic"),
    ("KinsectLevels", "kinsect", [19], "level"),
)


def make_lookups(
    sql_dir: Path, mapping: dict, reference: dict, language: str, language_index: int
) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for lookup_name, domain, equipment_types, variant in REFERENCE_LOOKUPS:
        for lookup in reference["lookups"][lookup_name]:
            for equipment_type in equipment_types:
                english = lookup["name"]
                chinese = KINSECT_TYPE_CN.get(lookup["value"]) if lookup_name == "SpecialValuesInsectglaive" else None
                rows.append({
                    "domain": domain,
                    "equipment_type": equipment_type,
                    "variant": variant,
                    "value": lookup["value"],
                    "name": chinese if language == "cn" and chinese is not None else english,
                    "english": english,
                    "source": KINSECT_CN_SOURCE if language == "cn" and chinese is not None else
                              (REFERENCE_FALLBACK_SOURCE if language == "cn" else REFERENCE_SOURCE),
                })

    for value, english in enumerate(reference["arrays"]["allArmorResistances"]):
        for equipment_type in range(1, 6):
            rows.append({
                "domain": "relic_resistance",
                "equipment_type": equipment_type,
                "variant": "armor",
                "value": value,
                "name": english,
                "english": english,
                "source": REFERENCE_FALLBACK_SOURCE if language == "cn" else REFERENCE_SOURCE,
            })

    # Rokumaehn/mh4edit stops its melee list at 0x14, but the independently
    # implemented mikewii/MH4U-Editor includes the normal 0x15 purple scheme.
    # Add the missing save value explicitly for all melee weapon types.
    for equipment_type in (7, 8, 9, 10, 13, 14, 15, 17, 18, 19, 20):
        rows.append({
            "domain": "sharpness",
            "equipment_type": equipment_type,
            "variant": "melee",
            "value": 0x15,
            "name": "紫斩方案 0x15" if language == "cn" else "Purple scheme 0x15",
            "english": "Purple scheme 0x15",
            "source": MH4U_SHARPNESS_SOURCE,
        })

    for spec in mapping["dex_lookups"]:
        for row in read_csv(sql_dir / spec["table"]):
            value = decimal(row[spec["id"]])
            if value < 0:
                continue
            name, english, fallback = localized_name(row, spec["name_prefix"], language_index)
            for equipment_type in equipment_type_values(spec["equipment_type"]):
                rows.append({
                    "domain": spec["domain"],
                    "equipment_type": equipment_type,
                    "variant": spec["variant"],
                    "value": value,
                    "name": name,
                    "english": english,
                    "source": source_name(DEX_SOURCE, fallback),
                })

    rows.sort(key=lambda row: (str(row["domain"]), int(row["equipment_type"]), str(row["variant"]), int(row["value"]), str(row["english"])))
    keys = [(row["domain"], row["equipment_type"], row["variant"], row["value"]) for row in rows]
    if len(keys) != len(set(keys)):
        duplicates = [key for key in sorted(set(keys)) if keys.count(key) > 1]
        raise ValueError(f"duplicate equipment lookup keys: {duplicates[:5]}")
    return rows


README_TEXT = """# MH4G save-editor data

This directory is generated by `tools/build_data.py`. Do not edit the CSV
files by hand.

- `cn/` uses the exact Simplified Chinese name arrays extracted from the
  game's `data/core_common.arc`; each array index is the save ID.
- `en/` uses English names in both `name` and `english`.
- Dex global IDs and third-party arrays are never used to assign item or
  equipment IDs. They only supply English labels, rarity and relic metadata.
- Relic appearance names are resolved through the explicit weapon-name
  crosswalk because those base appearances are not standalone Dex weapon rows.
  The exact game label is preserved; `is_relic` carries the relic marker.
- Relic armor names are resolved through an explicit series/part crosswalk and
  retain the exact game label. Placeholder IDs remain in CSV for lossless
  coverage; the editor hides them from ordinary selection lists.
- `rarity` is `0` when rarity is dynamic (relic/talisman) or the reference row
  has no exact Dex match. `is_relic` is `1` for known relic weapon and armor IDs.
- `equipment_type` in `equipment_lookups.csv` is the on-disk save type (`0` is
  reserved for global values; generated rows currently use concrete types).
- Melee sharpness value `0x15` and its embedded bar are cross-checked against
  the public `mikewii/MH4U-Editor` implementation. The MH4G-only overflow value
  `0xDA` remains explicitly marked as provisional in the editor UI.

The CCI, ARC and raw Dex runtime dump are intentionally not committed. Extract
`data/core_common.arc` from your own game, then rebuild with:

```bash
python3 tools/export_game_names.py /path/to/core_common.arc /tmp/mh4g-game-names.json --language cn
python3 tools/build_data.py --input D:/MH/DEX/mh4g-dex-raw --game-names /tmp/mh4g-game-names.json --output data
python3 tools/validate_data.py data --game-names /tmp/mh4g-game-names.json
```
"""


def build(
    input_dir: Path,
    game_names_path: Path,
    output_dir: Path,
    mapping_path: Path,
    reference_path: Path,
    weapon_crosswalk_path: Path,
    armor_crosswalk_path: Path,
) -> None:
    mapping = read_json(mapping_path)
    reference = read_json(reference_path)
    weapon_crosswalk = read_json(weapon_crosswalk_path)
    armor_crosswalk = read_json(armor_crosswalk_path)
    game_export = read_json(game_names_path)
    if game_export.get("format") != GAME_EXPORT_FORMAT:
        raise ValueError("unsupported or invalid game name export")
    if game_export.get("language") != "cn":
        raise ValueError("MH4G data generation requires a Simplified Chinese game name export")
    game_tables = game_export.get("tables")
    if not isinstance(game_tables, dict):
        raise ValueError("game name export has no tables object")
    if weapon_crosswalk.get("format") != "mh4g-weapon-name-crosswalk-v1":
        raise ValueError("unsupported or invalid weapon name crosswalk")
    if armor_crosswalk.get("format") != "mh4g-armor-name-crosswalk-v1":
        raise ValueError("unsupported or invalid armor name crosswalk")
    sql_dir = input_dir / "direct_sql" if (input_dir / "direct_sql").is_dir() else input_dir
    raw_manifest_path = input_dir / "manifest.json"
    if not raw_manifest_path.is_file() and sql_dir.name == "direct_sql":
        raw_manifest_path = sql_dir.parent / "manifest.json"
    if not raw_manifest_path.is_file():
        raise FileNotFoundError(f"raw dump manifest not found below {input_dir}")
    raw_manifest = read_json(raw_manifest_path)
    if raw_manifest.get("format") != "mh4g-dex-runtime-dump-v1":
        raise ValueError("unsupported or invalid raw dump manifest")

    item_message = mapping["tables"]["items"]["game_message"]
    if item_message not in game_tables:
        raise ValueError(f"game name export is missing {item_message}")
    if len(game_tables[item_message]) != mapping["tables"]["items"]["game_count"]:
        raise ValueError(
            f"{item_message}: expected {mapping['tables']['items']['game_count']} rows, "
            f"got {len(game_tables[item_message])}"
        )
    for spec in mapping["equipment"]:
        names = game_tables.get(spec["game_message"])
        if not isinstance(names, list):
            raise ValueError(f"game name export is missing {spec['game_message']}")
        if len(names) != spec["game_count"]:
            raise ValueError(
                f"{spec['game_message']}: expected {spec['game_count']} rows, got {len(names)}"
            )

    parent = output_dir.resolve().parent
    parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".mh4g-data-", dir=parent))
    file_stats: dict[str, dict[str, object]] = {}
    core_tables = {
        spec[key]
        for spec in mapping["tables"].values()
        for key in ("data", "names")
        if key in spec
    }
    core_tables.update(spec["table"] for spec in mapping["dex_lookups"])
    raw_table_counts = {
        filename.removesuffix(".csv"): len(read_csv(sql_dir / filename))
        for filename in sorted(core_tables)
    }
    decoration_spec = mapping["tables"]["decorations"]
    raw_decoration_rows = read_csv(sql_dir / decoration_spec["data"])
    unique_decoration_items = len({row[decoration_spec["item_id"]] for row in raw_decoration_rows})
    filters: dict[str, object] = {
        "items": "ID is the zero-based msg\\itemName_jpn game-resource array index; Dex Hex only joins English metadata",
        "skills": "SklTree_ID > 0 plus save value 0 (None)",
        "decorations": "save-local allJewels order; save value 0 is None",
        "equipment": "ID is the zero-based game-resource name-array index; Dex/reference data only supplies English and metadata",
    }
    match_stats: dict[str, dict[str, int]] = {}
    try:
        (temporary / "README.md").write_text(README_TEXT, encoding="utf-8", newline="\n")
        for language, language_index in mapping["languages"].items():
            language_dir = temporary / language
            items, excluded_items = make_items(
                sql_dir, mapping, game_tables[item_message], language
            )
            filters["items_excluded_rows"] = excluded_items
            datasets: dict[str, tuple[tuple[str, ...], list[dict[str, object]]]] = {
                "items.csv": (BASE_COLUMNS, items),
                "skills.csv": (BASE_COLUMNS, make_skills(sql_dir, mapping, language, language_index)),
                "equipment_types.csv": (
                    BASE_COLUMNS,
                    [
                        {"id": identifier, "name": cn if language == "cn" else english, "english": english, "source": SAVE_FORMAT_SOURCE}
                        for identifier, cn, english in EQUIPMENT_TYPES
                    ],
                ),
                "talismans.csv": (EQUIPMENT_COLUMNS, make_talismans(reference, language)),
                "equipment_lookups.csv": (LOOKUP_COLUMNS, make_lookups(sql_dir, mapping, reference, language, language_index)),
            }
            decorations, decoration_unmatched = make_decorations(sql_dir, mapping, reference, language, language_index)
            datasets["decorations.csv"] = (BASE_COLUMNS, decorations)
            if language == "cn":
                filters["decoration_reference_names_without_exact_dex_match"] = decoration_unmatched

            for spec in mapping["equipment"]:
                rows, stats = make_equipment(
                    sql_dir, mapping, reference, weapon_crosswalk, armor_crosswalk,
                    game_tables[spec["game_message"]], spec, language, language_index
                )
                datasets[spec["file"]] = (EQUIPMENT_COLUMNS, rows)
                if language == "cn":
                    match_stats[spec["file"]] = stats

            for filename, (columns, rows) in sorted(datasets.items()):
                count = write_csv(language_dir / filename, columns, rows)
                relative = f"{language}/{filename}"
                file_stats[relative] = {"records": count, "sha256": sha256(language_dir / filename)}

        source_files = [
            {"name": row["name"], "size": row["size"], "sha256": row["sha256"]}
            for row in raw_manifest.get("sourceFiles", [])
        ]
        manifest = {
            "format_version": "1.0.0",
            "dex": {
                "product": "MH4G Dex v1.0 Build 7",
                "runtime_dump_format": raw_manifest["format"],
                "source_files": source_files,
            },
            "files": dict(sorted(file_stats.items())),
            "filters": filters,
            "generator": {"name": "tools/build_data.py", "version": GENERATOR_VERSION},
            "game_resource": {
                "export_format": game_export["format"],
                "export_sha256": sha256(game_names_path),
                "language": game_export["language"],
                "source": game_export.get("source", {}),
                "tables": {
                    item_message: len(game_tables[item_message]),
                    **{
                        spec["game_message"]: len(game_tables[spec["game_message"]])
                        for spec in mapping["equipment"]
                    },
                },
            },
            "languages": ["cn", "en"],
            "mapping": {"format": mapping["format"], "sha256": sha256(mapping_path)},
            "match_statistics": dict(sorted(match_stats.items())),
            "raw_table_counts": raw_table_counts,
            "reference": {
                **reference["source"],
                "dataset_format": reference["format"],
                "dataset_sha256": sha256(reference_path),
            },
            "weapon_name_crosswalk": {
                "format": weapon_crosswalk["format"],
                "sha256": sha256(weapon_crosswalk_path),
            },
            "armor_name_crosswalk": {
                "format": armor_crosswalk["format"],
                "sha256": sha256(armor_crosswalk_path),
            },
            "row_accounting": {
                "items": "all msg\\itemName_jpn slots are emitted, including 0, DUMMY and no-item placeholders",
                "skills": "exclude the ID_SklTree_Name -1 placeholder and add save value 0",
                "decorations": (
                    f"DB_Jew has {len(raw_decoration_rows)} recipe rows and {unique_decoration_items} unique item IDs; "
                    "output adds save value 0 (None) to those save-local decoration values"
                ),
                "weapons_and_armor": "every game-resource name-array slot is emitted; external references provide metadata only",
            },
        }
        (temporary / "manifest.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )

        expected_existing = {"README.md", "manifest.json", "cn", "en"}
        if output_dir.exists():
            unexpected = {entry.name for entry in output_dir.iterdir()} - expected_existing
            if unexpected:
                raise ValueError(f"refusing to replace data directory with unexpected entries: {sorted(unexpected)}")
            for name in expected_existing:
                target = output_dir / name
                if target.is_dir():
                    shutil.rmtree(target)
                elif target.exists():
                    target.unlink()
        else:
            output_dir.mkdir(parents=True)
        for entry in temporary.iterdir():
            shutil.move(str(entry), output_dir / entry.name)
    finally:
        shutil.rmtree(temporary, ignore_errors=True)


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="raw runtime dump root or direct_sql directory")
    parser.add_argument("--game-names", required=True, type=Path, help="JSON exported from the game's core_common.arc")
    parser.add_argument("--output", required=True, type=Path, help="generated data directory")
    parser.add_argument("--mapping", type=Path, default=script_dir / "data_mapping.json")
    parser.add_argument("--reference", type=Path, default=script_dir / "reference" / "mh4edit_save_ids.json")
    parser.add_argument(
        "--weapon-crosswalk",
        type=Path,
        default=script_dir / "reference" / "mh4g_weapon_name_crosswalk.json",
    )
    parser.add_argument(
        "--armor-crosswalk",
        type=Path,
        default=script_dir / "reference" / "mh4g_armor_name_crosswalk.json",
    )
    args = parser.parse_args()
    build(
        args.input.resolve(),
        args.game_names.resolve(),
        args.output.resolve(),
        args.mapping.resolve(),
        args.reference.resolve(),
        args.weapon_crosswalk.resolve(),
        args.armor_crosswalk.resolve(),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
