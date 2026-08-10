# MH4G Dex runtime dumper

This tool loads `MH4G Dex.exe` as a 32-bit .NET assembly and invokes the same
resource/password and database initialization methods used by Build 7. It then
exports the live `DataTable` objects and selected editor-related SQLite tables.
It does not modify any file in the Dex directory.

Run on Windows from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\mh4g-dex-dump\run_windows.ps1 `
  -DexDir D:\MH\DEX\MH4G_Dex_v1.0Build7 `
  -OutDir D:\MH\DEX\mh4g-dex-raw
```

The output directory is external research data and must not be committed. On a
repeat run, the runner only replaces a directory containing its own
`.mh4g-dex-dump` marker.

This implementation targets exactly `MH4G Dex 1.0 (Build 7)`. Obfuscated type
and method names are verified before invocation; a different build fails
instead of guessing an initialization entry point.
