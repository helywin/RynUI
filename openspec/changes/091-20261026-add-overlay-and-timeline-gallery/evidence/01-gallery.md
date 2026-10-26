# 091 Gallery 实施验收

## 源码与边界

基线为 `8056de67953ee73015c7559f1782033d85e08573`，规划提交为 `9caf00943392b94541dc3ad4265063637db4ca34`。本阶段追加 Popover 备注编辑、Message/Notification 操作台和 Timeline 处理记录，四个组件目录使用生产 anchor 直接进入实际样例。公开 API、Core、Theme、goldens 和 CMake 未改。

这是 Windows MSVC `windows-msvc-debug`、Ninja Multi-Config、最多 2 jobs 的平台通用 Recording 证据。完整 Gallery、native startup smoke、GPU/人工视觉、Linux、系统 IME 和 AT 不在本阶段通过范围；任务 3.1 的旧流程代表回归与独立 Astra 审查仍待完成。

## 实际命令与结果

在 ROOT 通过 VSDevShell 设置正式 MSVC 环境，执行以下命令。忽略目录 `out/091-msvc.ps1` 只负责设置环境、逐条保存输出和返回实际退出码；不改源文件。对应原始日志为 `out/091-main-focused5-build.log`、`out/091-main-focused5-focused.log`、`out/091-main-final-focused5-inventory.log`、`out/091-main-final-focused5-contracts.log` 及同前缀 contracts XML。

```text
cmake --build --preset windows-msvc-debug --parallel 2 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --overlays-timeline-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --inventory-only
ctest --preset windows-msvc-debug --parallel 2 --output-on-failure -R "^rynui\.(ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$"
python tools/update_ant_design_gallery_catalog.py --check
python openspec/changes/091-20261026-add-overlay-and-timeline-gallery/evidence/compare-baseline.py
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

以上最终执行均退出 0。正式构建实际重新编译 definition 和 frame TU 并链接三个目标；保留四条既有 MSVC warnings 和已知 VSDevShell `vswhere.exe` 提示。新流程完整业务路径及 Default/Dark/Compact × 1200/560 两种有限宽度全部通过；精确八项 contracts 为 8/8。格式检查 723 个自有源文件、0 failures；显式 OpenSpec 1.14.0 入口 doctor healthy、strict 88/88。PATH 中找不到 clang-format 的首次格式检查未执行成功，改为上述绝对路径后通过，不用首次结果代替验收。

OpenSpec 实际通过 `C:/Program Files/nodejs/node.exe` 调用 `C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js`，未安装或修改全局依赖。

## 真实流程与保留性

- 测试点击可见目录按钮，经导航请求及共用生产 anchor 到达样例；Popover 真实输入、Tab 到自身保存 Button、Enter 提交，反馈来自实际草稿。位置、箭头、禁用恢复及 Escape 保留编辑器身份。
- Message 同 key 从 Loading 更新 Success；Notification 首次挂载富输入，后续仅更新标题/状态及位置，检查 entry、description、editor 的完整身份和实际草稿。关闭/清空后通知输入与组件消失，外部入口仍可重开。
- Timeline 真实编辑/保存、反序、三种位置、loading、隐藏恢复，检查 keyed region、Input/ref 身份；无失焦隐藏取消 preedit，恢复后原草稿可继续编辑。
- 六组有限视口检查实际 glyph shader clip、原生 Button fill、HitTest 及外扩 focus outline coverage，区分窗口层与普通 document clip。
- 比较脚本确认 358 个旧 IDs 原序保留、33 个旧样例函数体及调用顺序原文不变、69 个非目标 support overlay 条目不变，Core/API/Theme/goldens/CMake 无差异。

最终 inventory：

```text
stable_ids=390 navigation=93 buttons=389 interactions=1439 visible_interactions=693 live_samples=295 theme_content=67 inputs=153 selects=24 modal_drawer=13 selection=47 menu=2 dropdown=1 collapse=1 segmented=3 empties=3 results=3 forms=3 form_items=6 date_pickers=2 time_pickers=2 breadcrumbs=2 steps=1 descriptions=1 skeletons=4 popovers=1 notice_holders=2 timelines=1
```

## 夹具修正与产物身份

初次严格 glyph 检查发现新标题的负左 bearing 超出分配区 1 px，给新增标题/反馈留 1 dp 左边距后恢复，未放宽 clip 断言。Button 场景检查加入实际 own surface/effect fill；类型名使用完整命名空间避免歧义。首次键盘保存查找误选旧 Steps 的同名 Button，改为按 Popover content owner 定位；新增保存/取消 Button 各留 6 dp 外边距容纳焦点装饰。所有旧样例正文保持不变。中间失败日志保留在 `out/091-main-focused1` 至 `focused4` 前缀，不作为通过证据。

最终三程序 SHA-256：

| 程序 | SHA-256 |
| --- | --- |
| `tests/Debug/rynui_token_gallery_frame_tests.exe` | `884C6D814F72ABC20346166133D64385BDD3EC41EFDAB6FB77DC13739D723F15` |
| `examples/Debug/rynui_token_gallery.exe` | `0CA4C241CE017836137C81F1755BCDEDB156174B8CA57C6E6D711D818CA5B837` |
| `tests/Debug/rynui_ant_design_reference_catalog_tests.exe` | `D6C4B2FB8B7FC0782C6076F0E39526BDA6B70131D46DB55C254A96E52131FE41` |

路径均相对 `out/build/windows-msvc/`。证据记录的是本次实际新构建及运行，不把后续只读审查执行已有程序描述为再次构建。
