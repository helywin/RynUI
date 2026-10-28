# 093 隔离 worktree 实施与验收

2026-10-26。本记录对应 `C:/Users/jiang/.t3/worktrees/RynUI/t3-165e0445`、分支 `t3/implement-gallery-workflow`，起点 `0f578ddebc3307f5d95807745b91ef29667d7b43`。Core 冻结为 `29154cda31f3a8e7c5da06b660182fc75a2697fc`，Gallery 保留性基线为 `6118f67728aede9985867377b122274ac86f31d7`；依赖发布依据见 [01-core-baseline.md](01-core-baseline.md)。

## 状态与边界

本地技术实施及要求的定向 Recording 已完成，最终 fresh focused 六组矩阵全部通过。task 2.1 和 3.1 保持 unchecked：两项文字还包含父的 fresh Astra、main 集成、发布与远端 SHA 核对，尚未执行，093 仍为 2/4、不能声称 all_done。本子任务不执行 Git 远端操作，不创建 PR、archive 或 children。

验证是 Windows MSVC、`windows-msvc-debug`、Ninja Multi-Config 的平台通用 Recording。没有运行完整 frame、journey 或 native smoke；不计为 GPU/真实窗口、人工视觉、Linux、系统 IME、系统字体或 AT 验收。

## 实现合同

- 在既有 Timeline 后追加三个独立样例函数及调用。业务数据使用 GalleryState Signals/Bindings；恢复按钮在 hidden/loading/disabled 分支外。
- Avatar 的原生 Input 更新真实头像文字，测量宽窄字形；可切换文字/图标、形状、尺寸及 Group max.count。128 名成员使用原生 overflow Popover，以公开 Component Token 将 popup 最大宽度设为 240 dp，使两个有限窗口均真实触发滚动。富内容 Avatar 保留 Input 与保存按钮。
- 两个 Statistic 分别展示真实 typed int64/uint64/double/String 和富 value Input；内置精度截断/补零、分隔符、formatter、loading 均由组件执行。反馈对原始 typed 值使用 `to_chars`，没有 double 固定六位小数损失。
- Rate 使用受控 accepted value、真实半星/清空、Left/Right/Home/End/Enter/Space、disabled/readOnly 与恢复。第五颗星提供真实富 Tooltip Input/保存按钮，检查连续指针穿越间隙、键盘进入、草稿和完整身份。
- 三目录经真实导航请求和共同 production anchor 直达各自样例。新控件按 owner/snapshot 定位；Date/Time 的两个精确 anchor 预期均追加新 case，原 fallback 与旧断言保留。
- 新 `--members-metrics-only` 已接入默认完整 frame 入口。纯 Avatar/Statistic 没有伪造焦点语义；原生 Input 检查实际焦点阴影四边，保存按钮及 Rate 检查实际 outline coverage 四边。
- ComponentId、NodeId、InteractionId、editor 均按完整 handle 比较，包含 generation；Group 比较所有 keyed inline/overflow 身份。实际调用并验证成员/指标 InputRef 的 `focus()`；Rate 使用绑定 ref、原生 Tab/指针及 editor 身份，未声称实测全部 ref 方法。

## 实际构建与检查命令

通过 `out/093-msvc.ps1` 导入 VS 2026 MSVC x64 环境，所有 build/output/log 写入本 worktree 的 `out/`。只读复用 `D:/code/RynUI/out/build/windows-msvc/_deps/*-src`，配置的 `FETCHCONTENT_SOURCE_DIR_` 为 SDL3、RYNUI_FREETYPE、RYNUI_HARFBUZZ、RYNUI_SHEENBIDI、RYNUI_UTF8PROC、RYNUI_DXC、RYNUI_SDL_SHADERCROSS、RYNUI_SPIRV_CROSS；没有复用 ROOT exe 或 build objects。

```text
cmake --preset windows-msvc -DFETCHCONTENT_SOURCE_DIR_<dependency>=<locked source cache>
cmake --build --preset windows-msvc-debug --parallel 2 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --members-metrics-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --inventory-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --overlays-timeline-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --date-picker-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --time-picker-only
ctest --preset windows-msvc-debug --parallel 2 --output-on-failure -R "^rynui\.(ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$"
python tools/update_ant_design_gallery_catalog.py --write
python tools/update_ant_design_gallery_catalog.py --check
python openspec/changes/093-20261026-add-member-and-rating-gallery/evidence/compare-baseline.py
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
node <explicit OpenSpec 1.14 entry> doctor --json
node <explicit OpenSpec 1.14 entry> validate --all --strict --no-interactive
git diff --check
```

Python 为 `C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe`；OpenSpec 通过 `C:/Program Files/nodejs/node.exe` 调用 `C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js`。未安装或升级任何全局工具。

## 运行记录

| 检查 | 实际时间（UTC+08:00） | 结果 / 秒数 | frame SHA-256 |
| --- | --- | --- | --- |
| 最终三个目标构建 | 21:28:05.790–21:28:20.198 | exit 0 / 14.410 | `84ED5A54AB0013A5E5F839109CB8387A4D7525486CBFCE90180D0DDCCBB09159` |
| 最终 focused | 21:29:24.750–21:37:01.587 | exit 0 / 456.839；六组全部通过 | 同上 |
| 最终 inventory | 21:29:35.169–21:30:16.174 | exit 0 / 41.008 | 同上 |
| Overlay/Timeline 代表 | 20:59:27.508–21:03:36.696 | exit 0 / 249.190 | `294EDA49BE8E735A0AA258C152A7250C85A75952A3926CED513DF4F7F81C6B5F` |
| DatePicker 代表 | 21:05:21.607–21:11:08.429 | exit 0 / 346.823 | `8AF1FC82C26FB13737ADBB1B0BA1C34D2938C6E964CB427F97B42F345D403210` |
| TimePicker 代表 | 21:13:54.890–21:21:33.653 | exit 0 / 458.764 | `92F3A7313B84304AF9AD42814E10835142E396D8BA24CF3C195676491168CF6B` |
| 最终八项 catalog contracts | 21:31:38.754–21:31:39.099 | 8/8 / 0.346 | reference catalog exe 见 artifacts.json |

最终静态收口由 `out/093-static.py` 逐命令保存实际退出码：clang-format 22.1.3 检查 **761** 个自有源文件、0 failures；generator check、保留性比较、doctor（healthy）、all strict（**92/92**）与 diff check 均 exit 0。OpenSpec 的既有 `skip_specs` INFO 原文保留，不计为失败。

三个旧代表各只运行一次。其后 frame 的改动限于新增 focused 夹具、诊断和字体 resolver；这三个旧代表函数没有再次修改，也没有把它们报告为在最终 hash 上重跑。它们实际执行了 Default/Dark/Compact × 1200/560 和原有业务断言；Date/Time 同时验证三条新增 anchor 的准确偏移与旧 fallback。

最终 inventory 为程序实测，不按新增数量推导：

```text
stable_ids=419 navigation=93 buttons=409 interactions=1480 visible_interactions=726 live_samples=302 theme_content=68 inputs=156 selects=24 modal_drawer=13 selection=47 menu=2 dropdown=1 collapse=1 segmented=3 empties=3 results=3 forms=3 form_items=6 date_pickers=2 time_pickers=2 breadcrumbs=2 steps=1 descriptions=1 skeletons=6 popovers=2 notice_holders=2 timelines=1 avatars=260 avatar_groups=1 statistics=2 rates=1
```

## 保留性与产物身份

[compare-baseline.py](compare-baseline.py) 实测基线仍为 36 个旧 `add_*`，现为 39 个；保留全部 390 旧 IDs 相对顺序、36 个旧函数体及调用顺序，70 个非目标 support 条目逐条相等。仅 Avatar/Statistic/Rate 三条改为如实 `partial`，生成目录与三 API 文档同步。

脚本逐 blob 比较 524 个冻结文件（包括全部 `.cmake` 和父 README/completion），与 `29154cda` 完全相同。冻结 manifest SHA-256 为 `590e0eb882d3cc65054244da790ec8b66ecaea1cae03b67cf1f12340773d6f9b`。同时检查所有改动/新增路径都在明确授权范围内。

当前 gallery exe SHA-256：`924BDA2041D0E6F75CF82B8BB096E2186E6681DC3B15C15DEAD7F21EBA0F4F81`；reference catalog exe：`188C88BF6112FD938FD806F9063B0C25F122B261866BFC4E8DE078C23FD92A67`。详细源文件 blob、程序时间/大小/hash 和原始日志 hash 记录于 `artifacts.json`。

## 失败、夹具修正与日志完整性

1. 首次 frame 编译的 `detail` 命名空间歧义改为 `ryn::detail`；没有改 Core。generator 首次未传操作参数，按实际 usage 改为 `--write`，后续 `--check` 通过。
2. focused attempts 1/2：Avatar snapshot viewport 是未平移的文档坐标（例如 y=40666.8），改用相同 content node 的真实平移 bounds；未放宽 glyph 裁剪。
3. attempt 3：默认 480 dp popup 已容纳全部成员，不满足“真实滚动”夹具条件。样例用公开 Popover Component Token 将该 Group popup 限为 240 dp；原滚动与全成员可见断言保留。
4. attempt 4：Rate 的窗口层 Tooltip 文本被错误计入星条分配区。按真实 window-layer 归属分别检查星条和富 Tooltip，保留各自 glyph/fill/hit 检查。
5. attempt 5：前一主题/宽度留下 Rate 的 100 ms 离开宽限，固定时间夹具没有推进。补充真实 pointer/focus exit、单调时钟 tick 及 deadline/Tooltip 清理断言；Group 每轮用真实向上滚轮回到开始位置。
6. attempts 6/7：旧 Gallery Fixture 固定返回 14 px font identity，Compact 下 `WWWWWW`/`iiiiii` 都被测为只能降到 1 px；输入确实是 `iiiiii`。最小单 Avatar + 原生 Text 夹具确认固定链连 12 px Text 都测为宽 24；生产 host 使用按字号 resolver。新 focused 专用 resolver 按请求 pixel size 缓存并加载锁定的 Latin/CJK 验证字体。复核结果：wide natural=66/font=3，narrow natural=18/font=12，viewport=23.3345，原生 12 px Text width=18，exit 0（0.483 秒）。临时诊断 flag 已移除，未修改 Core 或用短文本/弱断言绕过问题。
7. 原 PowerShell transcript 未捕获 native stdout。已通过本线程 T3 durable `command_execution` 项及分页 textOffset 恢复完整原始终端文本到 `out/093-recovered-native.log`，不是重新执行或人工合成；后续日志由 native tee 捕获。最终运行还通过 Python 合并捕获 stdout/stderr，避免 PowerShell 将 stderr 转为异常。早期 transcript 仍保留实际开始/结束与 hash，但不能单独作为完整 native 输出。

最终 frame 编译保留四条旧 warnings：三条 C4834、一条 C4244；初始整树构建另有 FreeType C4819、SPIRV-Cross C5051 和既有 Core/示例 C4834/C4456/C4100/C4189。VSDevShell 的 `vswhere.exe` 提示及最初 shell profile 的 `oh-my-posh` 缺失均保留；后续命令使用 `-NoProfile`，实际 MSVC configure/build exit 0。不声称零 warnings。

原始记录均在本 worktree：`out/093-configure.log`、`out/093-build.log` / `.native.log`、`out/093-recovered-native.log`、`out/093-focused-attempt1` 至 `attempt7` 的日志、`out/093-focused-final.log` / `.native.log`、各 inventory 日志、`out/093-overlays-timeline.log`、`out/093-date-picker.log`、`out/093-time-picker.log` / `.native.log`、`out/093-contracts.log` / `.native.log`、`out/build/windows-msvc/out/093-contracts.xml`、`out/093-static.log`。所有记录的实际产物对应性及 SHA 以 `artifacts.json` 为索引。
