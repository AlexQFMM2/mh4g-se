# mh4g-se

MH4G（3DS）存档修改器。使用与 MH3G、MHGU 统一的中文 Qt 管理界面，
左侧切换角色、道具箱和装备箱，底层使用 MH4G 存档结构与数据集：

- 加密 `user1`、`user2`、`user3` 的打开、校验和原路径原子覆盖；
- 人物名字、性别、发型、内衣样式、声音、金钱和 HR 的基础编辑；
- 1400 格道具箱的查看、搜索、ID/数量修改和清空；
- 1500 格装备箱的查看、搜索、基础编辑和“只看发掘武器”筛选；
- 武器、防具、护石分别使用专用编辑窗口，支持三个装饰珠及固定/内置标记；
- 发掘武器攻击/属性档、斩味代码及七色条、专属值、升级、稀有度、研磨、孔数和极限强化；
- 操虫棍猎虫等级、类型及八项实例值；轻重弩 `0x01` 原始改造位；
- 发掘防具防御/抗性档、升级、稀有度、研磨和孔数；
- 护石两组 16 位技能 ID、有符号技能点、孔数和未知值无损保留；
- 道具箱与完整 28 字节装备箱 CSV 表单的批量导出、校验和导入；
- 完整 28 字节装备记录查看；
- 修改正在穿戴的装备箱记录时，安全同步原本一致的当前装备副本；

斩味 `0x00–0x15` 已按公开 MH4U 工具的内置图表解码；MH4G 特殊值 `0xDA`
暂按其唯一红紫越界图显示，并在界面明确标为待实机确认。轻重弩限制解除/配件位的
最终含义仍待实机反向。攻击极限强化按 `+20` 真攻击计算，发掘升级按
`+0/+10/+20/+30` 真攻击的参考规律计算并在界面标记。随身道具栏不纳入。
所有编辑从完整 28 字节记录做字段补丁，未修改和未确认的字节原样保留。
已知范围外的存档构造值显示为“固定扩展代码”，不再笼统标为未识别；极限强化按高两位解码，
例如 `0xFF` 显示为“生命吸收 + 附加位 0x3F”，并保留完整原值。
修改前请备份完整存档目录。
程序不会自动生成备份，也不提供另存为；保存成功后会弹窗显示被覆盖的路径。

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

Qt 界面移植自本工作区的 `mh3u-se`，本项目按其 GNU GPL v3 许可证发布；
完整许可证见 `LICENSE`。
