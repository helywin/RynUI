# Astra R2 selection 焦点装饰收尾

起点为干净 `ec110f6e4c46ef3065f10cef4d448ba880875574`。父代理提供 P2：Descriptions content slot 直接放默认 Switch 时，焦点装饰错误使用 content clip。本轮只修复该路径及同类必要缺口；2.1/3.1 验收先取消勾选，通过后恢复，进度 5/5。不创建新任务，不合并 Skeleton/main，不修改 ROOT，不执行远端操作；全新独立 Astra 由父代理安排。

## 先复现再修复

仅加入两项回归、生产代码仍与起点完全相同时：

```powershell
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 4 --target rynui_descriptions_region_clip_tests
ctest --preset windows-msvc-headless-debug -R '^rynui.descriptions_region_clip_selection(_modal)?$' --verbose
```

`out/descriptions-r2-before.log`：**0/2，CTest exit 8，0.63 秒**。断言读取实际 rounded-effect scene、outline SDF、material alpha/opacity 和 ancestor hard clip 后的 coverage，不仅比较几何矩形。测试直接挂载默认 Switch/Checkbox/Radio/RadioButton；测试祖先布局只移动面板，使控件位于真实外部祖先左边界，不缩小控件。

| 修复前路径 | 原始 ring coverage | 可用 coverage | 外部 hard clip 后 | 移除 hard clip 后 |
| --- | ---: | ---: | ---: | ---: |
| Switch，普通 / Modal | 1 | 0 | 0 | 0 |
| Checkbox / Radio / RadioButton，普通 | 1 | 1 | 1（错误越界） | 1 |
| Checkbox / Radio / RadioButton，Modal | 1 | 1 | 0 | 1 |

## 最小生产改动

生产代码仅修改 `selection_component.cpp/.hpp`：`resolved_clip` 内部参数默认仍为 `ClipPurpose::content`；selection 的 focus/shadow effect 显式查询 `decoration`。将该查询用于共享 selection 装饰，补齐普通 Checkbox/Radio/RadioButton 原先缺失的外部祖先 clip。其余内容路径已审查：wave、RadioButton fill/border 及其零面积占位 effect、compact seam 均继续使用默认 content 查询，fill/content/hit 原路径不变。无 Theme、renderer ABI、公共 API 或布局框架修改。

修复后同一新增 Debug 命令：**2/2，0.63 秒**（`out/descriptions-r2-after.log`）。全部四类控件在普通和 Modal 路径的 coverage 均为 **原始 1、可用 1、hard clip 0、恢复 1**；还验证真实 keyboard focus、Space 事件、hit clip、恢复后的 bounds/组件身份与 slot 挂载次数。

## 最小受影响验收

[run-r2.ps1](run-r2.ps1) 保存正式 presets、Ninja Multi-Config、实际 HEADLESS portable 目标名与 `--parallel 4`。Debug 新增两项已通过，后续过滤掉它们，避免重复；Release 运行完整去重集合：

```powershell
& ./openspec/changes/081-20261025-add-native-descriptions/evidence/run-r2.ps1 -Configuration Debug -TestFilter '^rynui\.(descriptions_region_clip_(horizontal|bordered|vertical|vertical_bordered|rich)|portable\.(selection_component|switch_features|radio_features|modal_component))$'
& ./openspec/changes/081-20261025-add-native-descriptions/evidence/run-r2.ps1 -Configuration Release
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' -B tools/verify_selection_controls_source.py
```

Debug 旧集合 **9/9，10.03 秒**（`out/descriptions-r2-debug.log`），连同新增两项共 **11/11**；Release **11/11，4.57 秒**（`out/descriptions-r2-release.log`）。包含原 R1 四种 glyph 分区裁剪和 Button/Input 装饰回归，另选 selection、Switch、Radio features 与 Modal 既有目标；未重复历史大矩阵。selection source contract 单独执行一次通过。

clang-format **22.1.3，661 个自有源码，0 失败**；doctor root healthy；strict **82/82**；worktree/cached diff checks 通过。精确收口命令：

```powershell
& 'C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe' scripts/format-code.py --check --clang-format 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'
& 'C:/Users/jiang/AppData/Local/pnpm/bin/openspec.cmd' doctor --json
& 'C:/Users/jiang/AppData/Local/pnpm/bin/openspec.cmd' validate --all --strict --no-interactive
git diff --check
git diff --cached --check
```

本轮未修改或重生成 goldens；Theme 与五组 golden 相对 `ec110f6` 零差异，旧数据保留脚本复核通过。保留上轮实际证据，不新增大规模 executable hash 取证。仅声明本机 Windows MSVC headless 验收，不声明 GPU、Linux、系统 IME 或 AT 通过。保存一个本地英文 Conventional Commit 后停止，父代理负责后续复核与发布。
