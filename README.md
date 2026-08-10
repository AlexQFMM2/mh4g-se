# mh4g-se

MH4G（3DS）存档修改器。当前 v0.1 按 `mh3u-se` 的使用方式提供：

- 加密 `user1`、`user2`、`user3` 的打开、校验和另存为；
- 1400 格道具箱的查看、搜索、ID/数量修改和清空；
- 1500 格装备箱的查看、搜索和基础编辑；
- 装备类型、ID、等级/孔数和三个装饰品原始值修改；
- 完整 28 字节装备记录查看及高级原始替换；
- 简体中文和英文数据切换。

第一版暂不为发掘武器、发掘防具、猎虫、弩改造和极限强化提供专用控件。
基础编辑采用字段补丁，未显示的字节会原样保留。修改前请备份完整存档目录。

## Linux 构建

需要 Qt 5、C++17 编译器和 OpenSSL 3 运行/开发库：

```bash
qmake MH4GSaveEditor.pro
make -j
./bin/MH4GSaveEditor
```

运行核心回归测试：

```bash
tests/run-save-core-tests.sh /path/to/user1 /path/to/user2 /path/to/user3
```

## Windows 构建

安装 Qt 5 MinGW 和 OpenSSL 后运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build-windows.ps1 `
  -QtBin C:\msys64\mingw64\bin
```

成品位于 `release/windows/`。推送到 `main` 后，GitHub Actions 也会自动生成
Windows portable zip。

## 数据集

成品位于 `data/cn` 和 `data/en`，包括道具、技能、装饰品、护石、
五类防具、十四类武器及装备编辑枚举。装备文件使用存档本地 ID，
不是 Dex 的全局编号。

在 Windows 上先运行 Dex 导出器：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\mh4g-dex-dump\run_windows.ps1 `
  -DexDir D:\MH\DEX\MH4G_Dex_v1.0Build7 `
  -OutDir D:\MH\DEX\mh4g-dex-raw
```

再生成并验证成品：

```bash
python3 tools/build_data.py --input D:/MH/DEX/mh4g-dex-raw --output data
python3 tools/validate_data.py data
```

原始 Dex dump 不进入 Git。存档本地装备 ID 顺序参考了 MIT 许可的
`Rokumaehn/mh4edit`；来源、固定 commit 和许可证均保存在
`tools/reference` 与 `third_party`。
