# Native source observation 实施

2026-11-12，Linux/GCC。此独立阶段只增加 internal `FontRuntime::source(FontIdentity)` 的只读 source view，提供实际持有的 immutable font bytes、face index/family/style 与 bold/italic 标记。资源 bytes 的借用期限为本 owner thread、该 font generation 存续期间。Gallery 后续对 bytes 做 hash；Core 不依赖 Web/JSON、SDL 或 renderer，字体选择/加载/渲染流程不变。

- 正式 `linux-gcc`，`rynui_font_runtime_tests`，Debug → Release 串行，单次 `-j2`；`ctest --preset linux-gcc-{debug,release} --no-tests=error -R '^rynui\.font_runtime$'` 各 **1/1 通过**。新增控制确认来源 bytes 与实际输入一致、foreign thread 被拒、查询不改变 coverage 状态、退役 generation 不可查询；原字体测试保留。
- HEADLESS configure 的 include/link guards 通过。首次选用现有 `rynui_portable_font_runtime` 时，其通用登记会链接整套 components；为保持本阶段定向范围，仅停止自己 main BUILD 的该宽构建，未计 pass，改用上述只链接 font 的既有测试 target。未重跑全量 CTest。
- clang-format 22.1.3、OpenSpec 1.14.0 doctor/strict 与 diff check 通过；原始构建/CTest 日志和自有 scenario WIP 精确补丁/pre-post 哈希只留 ignored out。

109 仍 **5/61**。scenario reader、hello/receipt/checkpoint/complete、真实 dispatch 与 readback 桥接尚在实施和验证，本阶段不勾选 3.x，不报告 Web/native pilot、GPU 或 Windows 通过。


## Native finite 输入与观测阶段

平台通用实现运行于 Linux/GNU 正式 `linux-gcc-headless`，Ninja Multi-Config、同目录配置串行、单 build `-j2`。Gallery 内部有界 JSON/SHA/scenario reader、`--page-scenario`/`--page-observations`、真实 `pages.dispatch` bridge、完整 factory retained-root 边界及 hello/receipt/checkpoint/complete 已接线。默认普通 Gallery 的原回调/move 路径、组件 API、正常 pump 与 5 秒 callback 合同保留；新增 applied-state/font/device 查询只读，JSON/hash 留 Gallery，HEADLESS 无 Web/DOM/SDL/renderer 反向依赖。

新 protocol/input 与旧 finite options/runner 四项 CTest 已在 Debug/Release 通过；另定向回归 controller/sample-page/host。Debug 首轮 **6/7**，唯一 red 为主线旧 `gallery_sample_page_tests.cpp` 仍精确要求 476 legacy targets（HEAD 原文可复核），当前完整有序 inventory 已为 490。仅将该精确 fixture 常量改为 490，保留所有 identity/owner/alias 强 guard，Debug 定向补跑 **1/1**；Release 完整定向 **7/7**（45.90 秒）。未取 Data/Progress 后续 WIP，也未运行无关全矩阵。

实际 CPU bridge 验证 pointer、Tab/Enter、受控 accept/reject、真实 commit/重复 callback 故障及恢复、组件各自 eligibility、48 logical units wheel、retirement/stale query。真实 Input base/selected/placeholder views 共享 shaping 的 const 查询已回归；首次 GPU 暴露的 mutable `text_state(view)` 误用在观测器修复，Core 的只读 guard 未修改。byte/UTF-16 CPU 控制 **3/3**，覆盖多字节/代理对拆分拒绝；文本 view 查询显式记录 byte index 单位。

### 实际 Linux 单侧 GPU 记录

全部为 `linux-gcc-release`、hidden Vulkan/SPIR-V、1440×900、实际 DPR1、私有 Wayland/DBus；逐轮核指定 Mutter 与实际 Clutter 映射/hash，`desktop_fallback=false`，退出后自有 PID **0**。SDL device 自报告 NVIDIA GeForce RTX 5070 Ti Laptop GPU / NVIDIA 615.78.08。原始 JSONL/BMP/log/closure/nonce 仅在 ignored out。

- 原 source closure `2f9f10464671b52cd98468b1cd0f25b07b74d0a04ccf167523c6caeb5b70a193` / binary `615f277c361340985f6972b8d53ef828e63031a2a393db0574aa03967ee29962`：Button **18/18 actions、5/5 checkpoints、hello/complete、19 submitted frames、5 checkpoint fence readbacks、2 callbacks**，独立 native oracle 0 fail。随后 Input 首 checkpoint 因上述 mutable view 查询 **rc1**，只有 2 submitted frames，无 complete；Select 未运行。失败不覆盖，不算三例通过。
- 修 const 查询后的 closure `0a6a9242d2c3edff28db8f792357c4be1f851c07d0c578879af7224cba0ba232` / binary `b40667e774e0788df183354306c9972184fed88f78d08e2b1b7397a181b63e31`：按原冻 definition/font，仅 Input **14/14 actions、5/5 checkpoints、hello/complete、15 frames、5 fence readbacks、2 callbacks**；Select 首次 **24/24、6/6、hello/complete、25 frames、6 fence readbacks、6 callbacks**。均协议完整，实际执行 3.106 / 5.460 秒；Select native oracle 0 fail，Input 保留下面的一条定义 red。未重复 Button，也不把旧 source 证明拼成新 source 三例绿。
- normal compositor callbacks 与以上 hidden readbacks 分列。原 renderer `gpu_submissions` 计数在此 export 路径原样为 0；它不代替实际成功 readback/fence 数或逻辑 submitted frames。

### Input 选区定义迁移

原冻 case SHA `14d997a9dd742b450373fa9cc8c73cbc99d6b056d52cd5ab19ee6e770dd57337` 的 selected-all oracle 误以 scalar 数 4 作为 native caret，实际读到 11，机器保留 **1 red**。正式 InputRef / docs/input 合同是 UTF-8 byte offsets，独立编码 `中文A🙂` 为 **11 bytes / 4 scalars / 5 UTF-16 units**。该问题属于测试定义单位错误，后续 case revision2 依正式合同写明单位并使用独立编码长度；原 case/red 不改写。这不是 native 产品修复，也未宣称新定义 GPU 复跑通过。

本阶段仅能关闭完成的 native 协议/dispatch/builder 子项。完整 snapshot 生命周期/字体错误负例、最终 source 三例、Web paired、分层比较/校准、真实 fault 红绿、目录覆盖和 Windows 仍按 tasks 开放；上述单侧采集不代替它们。

最终 CPU 协议/API 差量（显式 byte 单位、checkpoint observer layer、只读 Select 查询、未实施 scripted policy 拒绝）仅定向复跑上述四项，Debug **4/4**（18.64 秒）、Release **4/4** 通过。clang-format 22.1.3（含新源显式检查）、OpenSpec 2.0.0 doctor/strict 与 diff check 通过。本提交只新增完成 **3.1–3.3**，109 **14/61**；3.4–3.6 与 4.x/平台 paired 项保持开放。当前 unit 标签/定义迁移后的源码未追加 GPU 批次，不回写已有 source-specific 证据。
