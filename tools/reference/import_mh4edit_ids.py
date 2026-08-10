#!/usr/bin/env python3
"""Import save-local equipment IDs from Rokumaehn/mh4edit.

The source project is MIT licensed.  This importer deliberately reads only the
static name/value tables needed to map MH4G save values; it does not copy the
editor implementation.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


SOURCE_REPOSITORY = "https://github.com/Rokumaehn/mh4edit"
SOURCE_COMMIT = "20d593a46163c9c34f6f90f368bfdfaf065c4af0"

ARRAYS = (
    "allJewels",
    "allSkills",
    "allArmorResistances",
    "allEqpGreatsword",
    "allEqpSns",
    "allEqpHammer",
    "allEqpLance",
    "allEqpLbg",
    "allEqpHbg",
    "allEqpLongswods",
    "allEqpSwitchAxe",
    "allEqpGunlance",
    "allEqpBow",
    "allEqpDualblade",
    "allEqpHuntinghorn",
    "allEqpInsectglaive",
    "allEqpChargeblade",
    "allEqpTalisman",
    "allEqpHeads",
    "allEqpChest",
    "allEqpArms",
    "allEqpWaist",
    "allEqpLegs",
)

LOOKUPS = (
    "PolishReqValues",
    "Skill1AmountValues",
    "Skill2AmountValues",
    "UpgradeArmorValues",
    "DefenseValues",
    "RarityValues",
    "HoningValues",
    "NumSlotsValues",
    "SpecialValuesChargeblade",
    "SpecialValuesSwitchaxe",
    "SpecialValuesSnsLance",
    "SpecialValuesGreatswordHammer",
    "SpecialValuesLongswordDualblades",
    "SpecialValuesGunlance",
    "SpecialValuesInsectglaive",
    "SpecialValuesHuntinghorn",
    "SpecialValuesLbg",
    "SpecialValuesHbg",
    "SpecialValuesBow",
    "UpgradeValues",
    "ModifierValuesMelee",
    "ModifierValuesRanged",
    "SharpnessValuesMelee",
    "SharpnessValuesBow",
    "SharpnessValuesLbg",
    "SharpnessValuesHbg",
    "ElementValuesLow",
    "ElementValuesMedium",
    "ElementValuesHigh",
    "StatusValuesLow",
    "StatusValuesMedium",
    "StatusValuesHigh",
    "ElementTypes",
    "KinsectLevels",
)


def csharp_string(token: str) -> str:
    """Decode the non-verbatim C# string subset used by the source table."""
    # The upstream lookup source contains literal tab characters inside a few
    # display strings. JSON requires those control characters to be escaped.
    return json.loads(token.replace("\t", "\\t"))


def extract_initializer(text: str, declaration: str) -> str:
    match = re.search(declaration + r"\s*=\s*new\s*(?:\(\s*\))?\s*\{", text)
    if not match:
        # Arrays omit ``new`` and use ``= {``.
        match = re.search(declaration + r"\s*=\s*\{", text)
    if not match:
        raise ValueError(f"initializer not found: {declaration}")

    start = match.end()
    depth = 1
    in_string = False
    escaped = False
    for index in range(start, len(text)):
        char = text[index]
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[start:index]
    raise ValueError(f"unterminated initializer: {declaration}")


def parse_array(text: str, name: str) -> list[str]:
    body = extract_initializer(text, rf"public\s+static\s+string\[\]\s+{re.escape(name)}")
    tokens = re.findall(r'"(?:\\.|[^"\\])*"', body)
    return [csharp_string(token).strip() for token in tokens]


def parse_number(token: str) -> int:
    token = token.strip()
    return int(token, 16 if token.lower().startswith("0x") else 10)


def parse_lookup(text: str, name: str) -> list[dict[str, object]]:
    body = extract_initializer(
        text,
        rf"public\s+static\s+List<[^>]+>\s+{re.escape(name)}\s*\{{[^}}]*\}}",
    )
    entry_re = re.compile(
        r"new\s*\(\s*(0x[0-9A-Fa-f]+|[0-9]+)\s*,\s*(\"(?:\\.|[^\"\\])*\")",
        re.MULTILINE,
    )
    rows = [
        {"value": parse_number(match.group(1)), "name": csharp_string(match.group(2)).strip()}
        for match in entry_re.finditer(body)
    ]
    if not rows:
        raise ValueError(f"no lookup rows found: {name}")
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="path to MonHunEquipStatic.cs")
    parser.add_argument("output", type=Path, help="output JSON path")
    args = parser.parse_args()

    text = args.source.read_text(encoding="utf-8-sig")
    payload = {
        "format": "mh4edit-save-id-reference-v1",
        "source": {
            "repository": SOURCE_REPOSITORY,
            "commit": SOURCE_COMMIT,
            "license": "MIT",
            "copyright": "Copyright (c) 2025 Rokumaehn",
            "file": "MonHunEquipStatic.cs",
        },
        "arrays": {name: parse_array(text, name) for name in ARRAYS},
        "lookups": {name: parse_lookup(text, name) for name in LOOKUPS},
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
