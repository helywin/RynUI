# 094 Fresh Astra R1 修复验收

## 本地范围与提交

- worktree：`C:/Users/jiang/.t3/worktrees/RynUI/t3-9bdf1a7f`；branch：`t3/9bdf1a7f`。
- 修复前 clean HEAD：`b106c0b089e1f5da5007fe18dbd56a1a90479d11`，parent：`a5d0b041e62755316ea13edf74744adf43967c78`。
- `cff3dd95f156561296351a15f3d452694d3c3010`：仅回归、四项真实 before 证据及既有 094 tasks；生产代码仍为 b106c0b0。
- `243abf57fa40311146aa4584d47967727edb3caf`：挂载事务、Text layout 清理、零面积输入边界、checkbox translation、wheel handoff 及增强回归。
- 本文件及原始最终日志另作本地证据提交；最终 clean HEAD/parent 在交接消息中给出。未访问远端、fetch/push、创建 PR/archive、children 或其他 worktree；未修改 ROOT、Gallery、README、全局进度或其他 WIP。

## 四项真实 before 与实现结果

| R1 项 | b106 生产代码的真实失败 | 修复与最终断言 |
| --- | --- | --- |
| 1 初次挂载异常 | [before](logs/r1-before-mount_rollback.log)：同一 Content 局部 catch 后 tree_count=1，半成品未回滚 | RAII 覆盖 owner 登记后的初次 getter、富 title/icon、订阅及首次协调；原异常重抛，按完整 ComponentId generation 退役。Text/typography LeafLayout 注册后立即登记清理。7 个场景检查 fallback 可点击、旧节点/layout/订阅/ref/session/block 清理、销毁 owner 或仅私有 Input 后同 ref 重绑的新代 Tree 保留。 |
| 2 已提交零面积 | [before](logs/r1-before-zero_area.log)：height=0 后 selection_delta=1、check_delta=1、仍聚焦 | 依据 place_generation 区分未布局与实际零宽/高，在布局提交后的 window-layer 协调阶段更新整树 block，先于 child popup/effect 绘制。检查两种零尺寸下行键盘、文本、composition、session、无焦点 popup、capture 清理，反复恢复身份/草稿/ref，foreign block 隔离，以及 blur 中同步替换 owner/同 ref。 |
| 3 checkbox 重复平移 | [before](logs/r1-before-translated_checkbox.log)：父 Modal 与自身均滚动，actual_fills=0、center_covered=0 | shape 使用未平移 node.bounds，add 仅应用一次 translation；world clip 仍按最终位置求交。默认/半选/全选各 actual_fills=4、center_covered=1，核对实际 rounded effect quad/clip/coverage、半选块、全选 glyph 与 hit，而非仅 snapshot bounds；Home/End 恢复保留身份。 |
| 4 wheel handoff | [before](logs/r1-before-wheel_handoff.log)：自然高度长 Tree 自身无 range，Modal scroll_y 仍为 0→0 | 自身 range=0 或该方向已到边界返回未消费，有 range 时自身滚动，并把焦点显露后的 scroll 钳制到实际上限。最终自然高度场景 Modal 0→48；自身 range、边界交还及 child/foreign Select popup 优先级均实际检查。 |

四份 before 均为新编译回归在未修复生产代码上的独立失败，CTest exit 8。保留 [初始 checkbox fixture 失败](logs/r1-fixture-translated-checkbox.log)，它是 fixture 的 already-focused 返回值处理问题，不计为产品缺陷复现。

首次修复后的 [Debug 21 项](logs/r1-after-Debug-initial.log) 为 19 通过、2 失败。新增清理断言暴露旧 Text layout slot；width=0 的无焦点 popup 在 Tree auxiliary 阶段清理前进入 Select effect 绘制。保留 [诊断 2 项](logs/r1-diagnostic-Debug.log)、[零尺寸诊断](logs/r1-diagnostic-zero-Debug.log)；临时 effect 数值输出已从源码撤回。随后 [两项定向重测](logs/r1-targeted-Debug-1.log) 为 1 通过、1 失败，后者是新增 fixture 同时把同 ref 绑定两个活 Input，已改为先销毁旧私有 Input 再重绑。[挂载与 Text 3 项定向重测](logs/r1-targeted-Debug-2.log) 全通过。最终代码再对两配置各运行下述完整定向范围一次；没有重跑无关矩阵或生成金样。

## 最终 Windows MSVC HEADLESS 验证

| 配置 | 既有 Tree | R1 新增 | 直接共享代表 | 最终合计 |
| --- | --- | --- | --- | --- |
| Debug | 13/13 | 4/4 | 6/6 | [23/23，13.47 秒](logs/r1-after-Debug.log) |
| Release | 13/13 | 4/4 | 6/6 | [23/23，6.41 秒](logs/r1-after-Release.log) |

保留全部既有 basic/controlled/rich/scroll/clips/boundaries/mount_lifecycle/lifecycle/validation/reactivity/theme_scene/model/theme 断言；共享代表为 Input、Select、Modal、Focus lifecycle、Text、Text frame。共 23 个独立 CTest、两个配置最终 46 次执行；此前失败及诊断运行另列，未混作最终通过数。

实际工作目录始终为上述 worktree。正式 preset `windows-msvc-headless`；缓存为 Ninja Multi-Config、MSVC、HEADLESS/RECORDING。CMake 只读复用原锁定 ROOT/out 第三方 source，字体、缓存、编译、程序与日志均在自身树。使用 [run-r1.ps1](run-r1.ps1)：

```powershell
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r1.ps1 -Action build -Configuration Debug
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r1.ps1 -Action test -Configuration Debug
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r1.ps1 -Action build -Configuration Release
& ./openspec/changes/094-20261026-add-native-tree/evidence/run-r1.ps1 -Action test -Configuration Release
```

driver 的实际 build 为 `cmake --build --preset windows-msvc-headless-{debug|release} --parallel 2 --target`，显式列出 4 个 Tree 与 6 个 portable 目标；实际 CTest 为同配置 preset，`-R '^rynui\.(tree_.*|portable\.(modal_component|select_component|input_component|focus_lifecycle|text_component(_frame)?))$' -V --timeout 45`。原始 [Debug build](logs/r1-build-Debug.log)、[Release build](logs/r1-build-Release.log)、[诊断阶段定向 build](logs/r1-build-Debug-targeted.log) 均保留。曾误用非 HEADLESS 的 Text target 名，Ninja 在构建开始前拒绝，改用上述 portable 目标后成功；各轮自身构建顺序执行，未启动重叠构建。编译仍有原有 `text_component.cpp:882` unused viewport 警告，未把无关告警清理混入修复。

- [format](logs/r1-format.log)：`Python310/python.exe scripts/format-code.py --check --clang-format 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'`，22.1.3，743 文件、0 失败。
- [OpenSpec version](logs/r1-openspec-version.log)：显式 `C:/Program Files/nodejs/node.exe` + 原指定 pnpm 全局 `@fission-ai/openspec/bin/openspec.js` 为 1.14.0；[doctor](logs/r1-doctor.log) healthy；[strict](logs/r1-strict.log) `validate --all --strict --no-interactive` 为 91 passed、0 failed。
- [保留合同](logs/r1-retained-theme.log)：`Python310/python.exe tests/verify_tree_theme_baseline.py design-tokens/ant-design/6.6.5/golden` 核对旧 37 IIFE 原文顺序、198 ordinal 及旧 typed 值。该脚本的 “fresh goldens” 是既有输出用词；R1 传入现有受控 golden，没有重新生成。另由 [verify-r1.py](verify-r1.py) / [artifacts 原始输出](logs/r1-artifacts.log) 确认公开 Tree API、Theme、五个 golden 与 presets 相对 b106 完全未改，并记录各文件 SHA256。
- artifacts 同时核对自身 CMake cache、四份 before 失败、两份 23/23 最终日志和全部 20 个本次目标 PE（每配置 10 个）的 x64 PE32+ 标记、SHA256 与默认 stack reserve=1048576 字节。未增栈。
- 日志仅转换 UTF-16/BOM 编码为 UTF-8，保留实际文本、行尾、诊断与失败；`git diff --check` 通过，未改写既有 before 证据。

## 验收边界

当前无已知未解决 R1 四项 finding，待父 fresh Astra R2 独立审查及 main 集成。未验收 main/Gallery、真实原生窗口或 GPU、Linux、系统 IME、AT；HEADLESS composition/session 模拟不替代系统输入与辅助技术验收。既有首版动态拓扑、异步加载、drag/drop、虚拟化、搜索、RTL、动画、DirectoryTree/Web 等排除范围保持不变。
