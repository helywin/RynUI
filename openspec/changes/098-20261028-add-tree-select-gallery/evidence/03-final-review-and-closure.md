# 098 最终复核与完成记录

098 约定的两个 TreeSelect Gallery 流程已完成。最终生产／测试源码为 `35503b0db9b449eee29c02fd562eb0aabb957b60`，直接 parent 为 `1377dc2d7f48a497321555d4a854b1b6b8ef9fe9`；两个阶段均已实际 push main 并核对远端完整 SHA。本次最终收口只更新文档和任务状态，不改生产、测试、生成目录或冻结 Core。

## 独立 Astra R1

审查配置起止读回均为 `gpt-6-astra / xhigh / serviceTier=default`。审核者直接读取 ROOT `D:/code/RynUI`，起止均为上述 `main` 提交、工作区干净；隔离 checkout 未替代 ROOT 源码或程序。完整阅读 098 规划、API 文档、两阶段证据、产物索引及累计差异后结论为 **CLEAN**：既定首版范围完整，未发现可达 P1/P2，无需追加生产修复。

确认真实目录导航、19 节点目标选择、独立选择／关闭 authority、outside 单次关闭与重试、备注保存、门禁恢复、必填 Form 和连续聚焦 reset。停稳键盘唤醒、实际 Tree 焦点、有限滚动及完整 generation 保持有有效断言。导航缓存、实际内容区 outline、控制区上移和 Forms 十五条导航期望的修正均未放宽原业务或裁剪断言。

本轮审核仅直接运行 ROOT 现有 MSVC Debug 程序，每入口一次：

| 入口 | 结果 | 秒数 |
| --- | --- | --- |
| `--tree-select-only` | Default/Dark/Compact × 1200/560 六组通过 | 260.664545 |
| `--inventory-only` | 精确盘点通过 | 27.2861978 |
| reference catalog 程序 | exit 0 | 未单独报告 |

frame EXE SHA256 为 `5a7afd8b40acb9caf81da1311a5405d1b10705119ec53bbdbf0af71e9f045da9`。这是独立 existing-binary 复跑，不是审核者重新构建。审查期间没有源码、任务、证据、Git 或远端写入。

## 父任务验证与保留性

父任务实际使用官方 `windows-msvc-debug`、MSVC／Ninja Multi-Config、最多 2 jobs 重新编译并链接。最终程序的 focused 六组、原 Form 业务与六组矩阵、单独 frame contract、inventory 和八项 CTest contracts 均通过，命令、耗时和原始记录见 [第二阶段](02-theme-matrix-and-regressions.md)。单独 frame contract 不等于默认完整 frame／journey。旧 Tree/AutoComplete 代表在独立 Forms 导航期望修正前的 EXE 上通过，其程序身份和未变函数体已单列，不冒称最终 EXE 重跑。

最终精确 inventory 为 **458 IDs、93 navigation、433 Buttons、1593 interactions、771 visible interactions、307 live samples、68 Theme content、163 Inputs、5 Forms/8 items、4 Popovers、3 Trees、2 TreeSelect**。

441 旧 IDs、41 旧函数体／调用序、72 非目标 overlay 及 557 冻结 Core/API/Theme/golden/renderer/RC/CMake/README blobs 保持。审核核对两阶段 32／34 份记录、源码 blob 指纹以及当前 7 个源码、3 个 PE、4 个 OBJ、2 个 LIB 的 hash／size／mtime；Ninja 和原始构建记录对应，PE 均为默认 1 MiB 栈。父格式检查为 clang-format 22.1.3、803 文件零失败，显式 OpenSpec 1.14 doctor healthy／strict 98/98；generator 和基线脚本通过。

最终文档阶段再次实际执行格式检查（803 文件零失败）、显式 OpenSpec 1.14 doctor（healthy）及 strict（98/98）、generator `--check`、冻结基线脚本和 `git diff --check`，均 exit 0；未重复已通过且源码未变的测试矩阵。命令为：

```text
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
node <已核实的 OpenSpec 1.14.0 入口> doctor --json
node <已核实的 OpenSpec 1.14.0 入口> validate --all --strict --no-interactive
python -B tools/update_ant_design_gallery_catalog.py --check
python -B openspec/changes/098-20261028-add-tree-select-gallery/verify-baseline.py
git diff --check
```

Python 使用 `C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe`；Node 使用 `C:/Program Files/nodejs/node.exe`；OpenSpec 入口沿用阶段记录的 pnpm 1.14.0 安装，不依赖 PATH 中其他版本。

## 完成边界

完成的是 098 约定的 TreeSelect Gallery 和平台通用 Recording 验证。未运行本批默认完整 frame／journey、native window／GPU、Linux、系统 IME 或 AT 验收，也不沿用旧批次平台结果。原生组件仍按 097 的固定单选首版边界标为 partial，不包含多选／搜索／异步／动态拓扑／富节点／虚拟化等排除项。

本次按用户要求完成剩余后停止开发，不增加新功能，不自动创建 PR 或 archive。最终文档和任务状态经校验后单独提交，只推送 main 并核对远端 SHA。
