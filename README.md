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
- 七格配装器、技能/防御/耐性计算、本地 `MH_LOADOUT v1 / mh4g` JSON 和事务式一键加入装备箱；
- 配装器武器/防具候选支持只看发掘装备，发掘实例直接进入对应高级编辑器；读取存档后可从
  独立装备箱弹窗按原页/格选择，查看三颗装饰珠并逐字节复制完整实例；
- 配装广场、装备/发动技能/合法性/发掘装备组合筛选、只读详情、发布、点赞和举报；
- 复用 MH3G 的个人中心与关于页，以及统一的名称型长列表输入筛选交互。

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
tests/run-loadout-core-tests.sh
tests/run-searchable-combo-tests.sh
QT_QPA_PLATFORM=offscreen ./bin/MH4GSaveEditor --smoke-test-loadout
QT_QPA_PLATFORM=offscreen ./bin/MH4GSaveEditor --smoke-test-account
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

运行时只读取 `data/mh4g.sqlite` 和 `data/manifest.json`，不再解析 `data/cn`、
`data/en` CSV。当前确定性数据库 SHA-256 为
`56914473f536b6ec6d13d58e44b8f7a855ed6d4ae0f90409859339574b8e1b69`，包含
4,835 件防具、2,849 件武器和 290 个装饰珠，以及装备技能、珠子技能、发动技能阈值和
结构化发掘参数 lookup。Windows 便携包也只携带 SQLite 与 manifest。

存档 ID、孔位、装备技能和 `is_relic` 以 `code.bin` 原生表为权威；防具发掘标记固定使用
`special_flags & 0x80`。Dex Build 7 只补充防御上限、适用性和发动技能关系，名称数组负责
中英文显示。未确认的发掘最大强化值保持 `NULL`，界面显示“—”，不以推测值填充。

在 Windows 上先运行 Dex 导出器：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\mh4g-dex-dump\run_windows.ps1 `
  -DexDir D:\MH\DEX\MH4G_Dex_v1.0Build7 `
  -OutDir D:\MH\DEX\mh4g-dex-raw
```

从自己持有的 CCI/RomFS 提取 `data/core_common.arc` 和解压后的 `code.bin`，完成旧 CSV、
原生装备表与 Dex 导出后生成并验证 SQLite：

```bash
python3 tools/export_game_names.py /path/to/core_common.arc /tmp/mh4g-game-names.json --language cn
python3 tools/build_data.py --input D:/MH/DEX/mh4g-dex-raw --game-names /tmp/mh4g-game-names.json --output data
python3 tools/research/export_native_equipment.py /path/to/decompressed-code.bin /tmp/mh4g-native-equipment
python3 tools/build_sqlite_data.py --native-dir /tmp/mh4g-native-equipment --dex-dir D:/MH/DEX/mh4g-dex-raw --output data/mh4g.sqlite --manifest data/manifest.json
python3 tools/validate_data.py data --no-samples
```

CCI、ARC、原始名称导出和 Dex dump 不进入 Git。Dex 与 MIT 许可的
`Rokumaehn/mh4edit` 只补英文、稀有度和发掘元数据；游戏数组负责 ID 和中文名。
来源、固定 commit 和许可证均保存在 `tools/reference` 与 `third_party`。

## 配装与合法性边界

MH4G 配装保存七条完整的 28 字节装备实例；三个装饰珠的固定标记、护石 16 位技能点、
操虫棍猎虫字段和未知保留字节均逐字节往返。配装查重只使用性别和七条实例记录，改名不会
绕过查重，不同发掘参数则属于不同配装。

合法性使用社区三态：合法、非法、不确定，默认不确定。它只影响展示与筛选，不阻止发布、
打开详情或一键导入。`contains_relic` 只按 SQLite 中原生 `(save_type, save_id).is_relic`
计算；护石不计入，普通 ID 即使携带非零发掘参数也不算发掘装备。结构损坏、未知 ID 和记录
类型不一致仍作为安全错误拒绝。

名称型长列表沿用 MH3G 统一组件：输入关键词后才显示匹配候选，点击候选或按回车才提交；
未确认文字失焦恢复，点击输入区不会强制展开完整列表，箭头仍可主动查看全部候选。

复选框使用应用内统一的白底边框、蓝色选中态和白色勾号，Windows 与 Linux 外观一致，不受
系统主题只绘制勾号而隐藏边框的影响。

Qt 界面移植自本工作区的 `mh3u-se`，本项目按其 GNU GPL v3 许可证发布；
完整许可证见 `LICENSE`。
