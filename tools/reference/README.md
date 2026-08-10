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
