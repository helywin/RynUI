# 固定 Web reference

2026-11-12，独立 Linux x64 worktree，actual base `b59442233a3963b2e14d47cf3ce6924105455bc2`，runtime 实核 `codex/gpt-6.1-sol/xhigh/priority`。只本地提交；不 fetch/rebase/push，不改 shared main。交接 patch/base absence/post hashes 均验证后才应用。

## 依赖准备阶段

- 2.1 已完成。52 个 direct pin 未变；冻结 98 个 pnpm 解析包及 pnpm 本身，共 99 个实际归档的 URL/integrity/SHA256/license、文件树及 pnpm 自带依赖。Node 22.21.1、pnpm 10.23.0、React/ReactDOM 19.2.0、Playwright 1.56.1、esbuild 0.25.12；Chromium headless shell 1194 / 141.0.7390.37，完整归档 SHA256 `d745c7210cc88b73c7991a22c9624819960d84bc3de77ac7336d2de841a8c91f`。Web Python 仅用 CPython 标准库。
- `bootstrap.py --mode BUNDLED --output <fresh-out> --offline`：从核验后的完整归档给新 loopback registry、新 store、新 node_modules 执行 `--frozen-lockfile --ignore-scripts`；实际 72 个当前平台包、`reused 0`。不是复制旧安装。SYSTEM 显式路径及精确版本/bytes 另测；没有系统优先或失败 fallback。
- `RYNUI_WEB_PREPARED=out/demo-differential/prepared-checked python -B -m unittest discover -s tests -p 'demo_differential_web_bootstrap_tests.py'`：**8/8 通过**，含 fresh BUNDLED/SYSTEM、缺依赖/错版本、实际连接失败、源码/demo/Token/tag/安装包篡改、归档截断、cache hash/环境身份负例。browser binary/完整资源树和实际共享库 hash 均核对。
- 全局 OpenSpec **2.0.0**：doctor healthy，strict **109 passed / 0 failed**；`git diff --check` 通过。启动时 list/status/apply 曾用只读复制的私有 1.14.0；用户升级后已改用 2.0.0，不改写旧版本记录或混入主线工具生成改动。

源码/安装/store/browser/profile/完整日志与 hash 索引仅在 ignored out；无 C++/HLSL 或原生 CMake 依赖修改，未运行无关 CTest/GPU 矩阵。此阶段不勾选其余 2.x，不声称 native paired、GPU、Windows、桌面 present 或系统 IME/AT 通过。

## Web 执行与文档阶段

- 2.2–2.6 已实施。固定源码树、5 个 demo、官方 version 生成输入、实际 bundler import closure 与组件执行身份均自动校验；`antd` alias 指向锁定源码。baseline/catalog 两个既有 checker 通过。没有修改官方 CSS recipe 或读取 native bounds 生成布局。
- 真实 Chromium 的 **16 个 Web test 方法均通过**：先跑完整 14 项，再完成新增 2 项与最终受影响项定向回归；其中 5 个原样官方 export 分别执行。覆盖实际 Button/Input/Select 与官方 Flex/Space/Typography composition、accept/reject、键盘/清除、disabled/loading、隐藏前位置真实输入、错误实际 Props/options/组件执行身份、缺 target、真实漏鼠标分发、缺帧/paint receipt/complete、外网拦截及字体 fallback。中文正控制实际选择锁定 `NotoSansCJKsc-Regular`；未用人工或 Agent 读图判定。
- `reference.py run` 的 prepare/font/identity/run 文档命令实际执行；fresh/cache-hit 的相同截图 hash 验证通过，命中仍运行全部动作并产生本次 nonce/receipt/capture。cache 使用只读原子发布、完整文件 hash、schema/字体/依赖/实际浏览器环境校验；GPU 身份错配实际非零。初始化耗时等诊断量不混入兼容身份。原始报告与 bundle 只在 out。
- Web 输入只消费共同 fixture 的公开 Props/data/slots/父策略和布局事实；composition-v1 限深 24/节点 512、控件恰一次，未知 DOM/CSS/JS 拒绝。hello 的初始 effective_inputs 固定；动态 value/proposed/focus/open/selection、callback `{proposed,committed,accepted}` 单独观察。strict b594 v1 的 hello/receipt/checkpoint/complete 全字段填写；不生成 oracle。
- 实际 Linux x64、headless Chromium 141.0.7390.37、DPR 1、Noto regular Latin/CJK；CDP 观察到 ANGLE/SwiftShader backend，字体使用浏览器实际 PostScript face 与已加载文件 hash 核对。JS timer/CSS 中间态 canary、独立 CSS 冻结、新 compositor screenshot 完成回执通过，PNG 实际 RGB/RGBA 编码如实记录，不标为 native GPU fence/present。
- 按用户最新明确指示，**不冻结 Python 解释器**：仅 CPython 兼容性检查、无第三方库及 actual `sys.version`（本机 3.14.8）记录；未提交额外的解释器精确 pin。未来实际使用的第三方 Python 比较库仍需锁定。
- 文档命令的 bootstrap **8/8**；协议回归 **5/5**，inventory **9 passed / 1 skipped**（本树未构建 native binary，未冒称其通过）。全局 OpenSpec **2.0.0** doctor healthy、strict **109 passed / 0 failed**，diff/artifact 检查通过。没有 C++/HLSL 修改，未重跑无关 Core/GUI 矩阵，107/108 向量未改。

本 owner 仅完成 Web 任务 **6/6**；此独立树 109 为 **11/61**。未运行主线统一三例的 native paired pipeline、native GPU/校准、完整动态时钟/IME seam/lifecycle、其他实际 DPI、Windows/MSVC/D3D12 或桌面/OS IME/AT。对应后续任务继续保留；Web 执行成功仅报告 `executed=true, paired=false`。

## 实际输入与自然高度收尾

- `effective_inputs.props.controls/data/slots` 改为从已提交 React fiber 的实际公开输入重建，Icon 按实际组件类型识别、caption 按实际 Typography 输入读取；父策略/Provider 与 DOM 容器另有实际查询。自动负控制篡改 wrapper 声明后，实际输入观察保持真实组件值，未生成 native expected。
- `container.height=0` 为自然高度，正数保持显式高度；不添加 overflow clipping。真实长上下文 wheel 使用可见交集派发并等待 trusted 事件，viewport readback 与全样例几何分开。两个新增真实测试及原 Button/Input/Select/composition 回归通过。
- 最终 **18 个 Web test 方法均通过**：完整运行前 16 项已逐项成功后被模型设置切换中断，仅续跑最后两项，**2/2** 通过；没有重复执行既有安装矩阵。续跑实际 runtime 为 `codex/gpt-6.1-sol/xhigh/serviceTier=default`，早期 priority 证据保留原记录。OpenSpec **2.0.0** doctor healthy、strict **109 passed / 0 failed**。
- 依 main 明确接手约定，后续完整 factory 适配仍由 main 统一扩展：局部 ConfigProvider theme、Select prefix/control slot 恰一次、Input statusPolicy 和 feedback Text 绑定。当前不支持的 recipe 字段明确失败，不删原上下文；本收尾不声称三例 paired pipeline 已通过。

## main 合入复核

主线从不可变 `82b8c3e85b8ac5f06306631aad5b0ae1cfd6a17c` 仅取上述准备阶段的 8 个路径；未读取后续未提交 adapter。main 自有 fresh `out/demo-differential/prepared-web82` 实际 BUNDLED/offline 完成锁定归档安装，Chromium 141.0.7390.37 可执行；同命令 bootstrap tests **8/8**（25.873s）、协议 **5/5** 通过。解释器按用户最新指示只检查兼容性、记录实际 `sys.version`，不增加解释器 exact pin/hash/license 门槛；第三方库/Node/pnpm/browser/source 的既有精确锁定不降级。OpenSpec 2.0.0 doctor/strict **109/0** 与 diff check 通过。仅 2.1 新增完成，主线 **6/61**；原始文件均留 ignored out，未报告 native paired。

## main 固定 Web adapter 合入复验

主线仅语义接入 `15b14611a57b9a796509e7326946ea9172024213` → `afeb9d00f488564226ae302652bc6351f9a3ac9e` 相对已合入 82b8 的 8 个路径；保留上节 main 准备证据，不复制父链。实测 identity 与 afeb 一致：app SHA256 `192d5a384a08c3a918d7326f5f022370d8b34a2c59a716394fed031fe92cbefc`，code closure `bfeab550e9d64eaa962444eee07720c7e87750d43295ea290c22bd46b371e7e4`。

main 自有 prepared82、两份 central-lock Noto 字体实际字节，独立 headless Chromium 上 `demo_differential_web_adapter_tests.py` **18/18**（175.786 秒）；实际覆盖五个官方 export、真实输入/清除/受控策略、committed React fiber Props/data/slot 来源、自然高度、wheel、缓存及环境/漏动作/图像/结束负例。首次字体准备曾因 SSL EOF 失败，另一次错误传入字体目录而非 JSON 文件使测试未进浏览器；原始失败保留，正确参数和已验证字体字节复跑后全通过，无放宽断言。退出后自有 browser/worker PID **0**，无用户桌面/浏览器。

OpenSpec 2.0.0 doctor healthy / strict **109/0**、协议 **5/5**、diff check 通过；仅 2.2–2.6 新增勾选，主线 **11/61**。完整 factory 的 ConfigProvider/Select prefix 与嵌套 control slot/Input status 与反馈绑定是后续集中集成工作，尚未在本阶段宣称支持；三例 native/paired/fault 红绿、校准、完整目录及 Windows 仍未通过。原始 log/截图/请求/环境索引仅在 ignored out。
