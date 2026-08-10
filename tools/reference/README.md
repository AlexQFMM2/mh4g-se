# Save-local ID reference

`mh4edit_save_ids.json` contains the save-local equipment order and the
equipment-editor lookup values imported from
[`Rokumaehn/mh4edit`](https://github.com/Rokumaehn/mh4edit), commit
`20d593a46163c9c34f6f90f368bfdfaf065c4af0`.

The upstream project is MIT licensed. Its license is preserved in
`third_party/mh4edit-LICENSE.txt`. Only static names and numeric lookup facts
are imported; the application implementation is not copied.

To reproduce the reference JSON from an upstream checkout:

```bash
python3 tools/reference/import_mh4edit_ids.py \
  /path/to/mh4edit/MonHunEquipStatic.cs \
  tools/reference/mh4edit_save_ids.json
```

## MH4G weapon-name crosswalk

`mh4g_weapon_name_crosswalk.json` fills two gaps that cannot be resolved by an
English-name join alone:

- 41 ordinary save IDs are linked to Dex weapon IDs after exact comparison of
  the Japanese names. The generated Chinese and English names still come from
  MH4G Dex Build 7.
- Relic appearance names do not exist as standalone rows in the Dex weapon
  table. Their compact Chinese names are recorded explicitly. Names shared with
  `mh3u-se` retain that GPL-v3 source marker; the rest use Dex series terminology
  or a reviewed Japanese-to-Chinese name crosswalk.
- The 435 relic save IDs are recorded as explicit per-weapon-type ranges. A
  parenthesized color in an ordinary weapon name (for example `Shotgun
  (Green)`) is therefore never mistaken for a relic marker.

The upstream `MHsavEditor` equipment list was used only to verify factual
save-ID/Japanese-name correspondence. Its code and data are not redistributed.

## MH4G relic-armor name crosswalk

`mh4g_armor_name_crosswalk.json` supplies reviewed Simplified Chinese series
and part-name mappings for relic-armor appearance IDs. These save-local names
do not exist as standalone rows in the Dex armor table. The mapping is exact
and deterministic: it recognizes only the listed series and suffixes and does
not use fuzzy matching. Generated names include `（发掘）` so they cannot be
confused with normal craftable armor. Placeholder `DUMMY` rows remain in the
dataset for lossless save coverage and are hidden by the UI.
