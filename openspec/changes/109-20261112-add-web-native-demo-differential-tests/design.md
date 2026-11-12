# Design

## Context

动机见 [proposal](proposal.md)。以下设计基于独立 checkout `48033d58061a540514ed7f60941e9880fc9b18d5` 的只读检查；本次没有构建/启动 Gallery、运行浏览器、采集 GPU 图像或实现差分工具。

### 已核对的现状

| 来源 | 实际能力与本设计使用方式 |
| --- | --- |
| `gallery/ant-design/6.6.5/{source-manifest,support-overlay}.json`、`tools/update_ant_design_gallery_catalog.py` | 固定 6.6.5/`4a39f54842eade4e565ab336ef6097cd7e723cdd`，73 目录项；本 checkout 状态为 implemented 56、partial 15、deprecated 1、web-only 1。目录状态不是测试通过状态。 |
| `gallery_samples()`、`tools/generate_gallery_source.py::generate()` | 真实 writer 产生 114 records/114 unique，包含 110 个组件 sample 和 4 个 Token sample。旧视觉 runner 的正则发现 75 组件/家族路由、71 个直接组件；它漏掉非组件 Token 范围，不能作为唯一 inventory。 |
| `gallery_test_inventory.cpp` | 静态枚举 490 targets/490 unique，含 sample owner/alias；完整有序身份与研究记录仅在本地 ignored out。它们是该 SHA 的观察，不写为未来总量上限。 |
| `tools/run_gallery_visual_acceptance.py` | 图像裁剪/PNG、空白与往返告警、contact sheets；一般 verdict 仍为 `pending image inspection`，Drawer 数值与 Notice loading 有专门判定。无通用 Web oracle。 |
| `tools/run_gallery_page_acceptance.py`、`gallery_page_acceptance.{hpp,cpp}`、参数及 Python runner tests | 有有限进程/超时/JSON result、page/sample/宽度/locale/direction 参数、backend/font 校验；Linux 会包装私有 Wayland，不能直接套旧视觉脚本在用户 session 启动。 |
| `gallery_pages_runtime.cpp` | `GalleryPageEvents`/`GalleryPageSubmitter` 使用正常 `pages.dispatch`、scroll 路由、`OnDemandFrameLoop`、`acceptance_time`、共享 resources/renderer，hidden 路径 `save_frame_bmp`；支持 core composition/session seam，部分 helper 会直接 request_focus，不能把它标成 Tab 或 pointer 命中证明。 |
| `gallery_visual_capture.hpp` | 逐 sample/分段/往返采集，激活上限 8、部分动画 wall sleep；这些是旧巡检行为，不能充当声明动作全执行或确定时间采样。 |
| `tests/gallery_visual_regression_tests.cpp` | 对真实 retained scene/节点/文本/clip 的定向合同与部分 renderer reference；无浏览器参考、无全局像素等价证明。 |
| 108 i18n、`gallery_i18n_acceptance.cpp` | zh-CN/en-US、独立 format locale 与 direction，实际 finite 语言切换和局部图像变化；语言不隐含 RTL，不能将“图像有变化”作为翻译内容正确 oracle。 |
| `docs/renderer-contract.md`、`docs/architecture.md` | logical scene v3、packed GPU ABI v2、SceneDeviceMetrics；当前颜色编码数值域与 alpha 合同不声明浏览器像素一致；默认系统字体，显式文件优先，验证字体只作明确配置。 |

`docs/reviews/2026-11-06-gallery-visual-audit.md` 明确“采集成功不等于视觉通过”，其旧问题之后已有修复；`2026-10-29-implementation-tuning-audit.md` 是更早冻结审查，借用其几何/Token/状态与 GPU 分层方法，不把旧失败或样例数量当现状。

root 的交接澄清：main writer 也是 114/490，112 是旧 test 字面值；新增 source 来自既有 `card.rounded-mask`/`modal.backdrop`，并非 Progress 比 main 多 2。随后只读 `git show 521a3e5e6005b5bd3b00e32cb066d38b6b4b1199` 核对了已冻结的 `gallery_context_acceptance.{cpp,hpp}`、runtime include/call 和 `evidence/application-context-linux.md`：`accept_application_context(route, ExistingDataEntryActions)` 复用 render/reveal/activate/click/pointer/press，按本页 owner 退役真实 capture/Core preedit，保留 shell Drawer。该提交的记录是正常泵 6 cases/381 checks/80 submitted=80 callback markers，独立 hidden 4 cases/57 checks/171 frames/72 captures；这是已提交证据的读取，本轮没有复跑，不转为 109、Web 或 Windows 通过。

后续 root 交接 main `a45a83e85524e3662fd098daf34bbb9cd3af2942` 仅更新非 Progress 反馈目录状态与 guard，inventory 不变；其运行数量未由本轮复跑，109 不依赖它的测试通过声明。实施前在选定冻结集成基线上重新枚举完整顺序和源码差量，保留旧身份与新增消费者检查，不扩大全局豁免。105/106 余项是各自平台验收，不作为 109 每次小修的全矩阵门槛。109 不修改 107/108 checkbox，不接管独立 Progress 数值研究/编译器修复。

## Goals / Non-Goals

**Goals:** 让同一命令给出可复现的机器 pass/fail；每个失败都有确定的 case、检查点、证据和重跑范围；先证明小 pilot 真能抓住故障，再扩展。覆盖集合与实际二进制相互校验，数据源不靠 Agent 每次重新判断。

**Non-Goals:** 不承诺任意 HTML 与 native 完全等价，不比较官网文档壳，不让截图感知模型充当裁判；不改变正式组件视觉入口、生产 Core 依赖、帧泵或上传框架；不把合成 IME/DOM 事件写成系统 IME/AT/实际桌面呈现验证。

## Decisions

### 1. 唯一覆盖账本与稳定身份

实现 `tools/run_demo_differential.py`（编排）和 `tools/demo_differential/`（纯 Python schema/选择/比较/报告），数据输入放 `tests/fixtures/demo-differential/`。独立 `tools/demo_reference_web/` 只负责官方参考执行。以下路径/CLI 均为待实现接口，本规划未创建它们。

`coverage.json` 使用 JSON Schema 2020-12，保存**声明性覆盖关系**，不保存运行状态或截图结果。自动 inventory 包含五个保持原始顺序的向量：catalog canonical entries、page/family routes、sample descriptors、source records、legacy targets（含 owner/alias）；Token 内建页与 List/Util 说明页单列而不增加 canonical 总数。新增 native `--page-inventory=<out/path.json>` 在不开窗口的有限模式导出编译 registry，Python 重新调用当前 generator，逐项比较身份、顺序、owner、source function/header 和 alias；只检查数量/子集的 guard 不足。

每个 ledger row 关联 `component_id + family + sample_id + target_id + feature_id`，可以映射多个 `official_demo_id` 和多个 case；一个官方 demo 可以贡献多个 native sample 的明确子合同。case identity 采用 `antd665.<component>.<scenario>.v1`，与数组下标、文件行号、语言文字和显示标题无关；profile/locale/DPR/state 是 run variant，不用复制身份。测试 target 的 rename 必须有迁移映射，失效 alias、循环 alias、重复身份、静默重排均失败。

coverage 分三条正交轴：`mapping=paired|native-only|web-only|unsupported|pending`、`implementation=ready|planned`、当次 `execution=pass|fail|error|not-run`。`pending/planned/not-run` 不能进入 pass。`--suite pilot` 只声明 pilot 分母；`--suite all --coverage required` 要求当前全部 applicable obligations，任何未接入项使结果不完整。初期可提交部分可验证阶段，但不能将 pilot 通过称为全覆盖。

全量 obligations 自动从当前 inventory 和版本化 feature matrix 联结：每个 live sample 至少有 default 可见区域检查，每个 target 必须被明确声明为 action/read-only observation/alias，适用的 disabled/loading/受控/弹层/滚动等维度必须有 case。feature matrix 依据具体 pinned 组件合同，不用所有 Props 的笛卡尔积，也不能由 adapter 自报“无需验证”。缺字段使用显式 `not-applicable` 和源码依据，不能默认 false。

List 保留迁移说明页自动合同；Util 的 GetRef/GetProps/GetProp 是 TypeScript 类型工具，验证来源说明/无伪控件。native-only 如 page generation、Scene epoch、非 Web 文件服务，仍执行独立合同并报告单侧通过。官方 Web 专属能力只豁免具体维度；不能把“当前 native fail”“没有 adapter”“没有 oracle”解释为 unsupported。

### 2. 固定官方参考：原样 demo 与等价参数化双通道

Web reference 不访问漂移官网。prepare 下载官方 commit 源码至 `out/demo-differential/cache/sources/<sha>/`，核对归档 SHA256、package version、demo/import closure、token lock 记录、资源及 license。现有 `audit_ant_design_665.py` 是版本迁移审查，不是执行器；复用其来源校验方法与现有 baseline checker，不扩大历史 6.5.0 比较。

参考使用该 commit 的真实 `components/` 代码经测试 bundler 编译，`antd` alias 指向此源码；不手写 CSS 重画 RynUI，不使用“当前 npm latest”。测试 package 的完整依赖解析由独立 lock 冻结；上游 `package.json` 本身含大量 `^/~`，单独锁 `antd=6.6.5` 不足。prepare 先下载/校验并安装，execute 阶段禁止非本地网络，图片/日期/上传返回使用声明的确定性 fixture。

两种 reference 需在报告中显式区别：

1. `official-demo`：直接导入官方 demo export，在受控外层容器中运行；不改 demo 内部样式或 callback。它提供来源可执行性/行为探针，native 有相同条件时可直接 paired。
2. `official-component-adapted`：当前 native 样例是多控件组合、中文业务数据或受控状态与官方 demo 不同，则用该官方组件及明确 demo 配方构成测试 wrapper。允许的差异仅为公开 props、数据、slot 内容、容器约束和受控父状态策略；逐字段写 `adaptation`。不允许覆盖组件 CSS 配方、修饰截图、删除 DOM 内容或读取 native 实测几何生成 Web 样式。原样 demo smoke 与适配场景共享同一官方模块，并校验 module/source identity。

case 保存 `official:{repository,commit,demo_path,demo_sha256,source_paths,adapter_id,adapter_sha256,adaptation}` 与 `native:{route,family,sample_id,targets,source_function,public_header}`。两个 adapter 都消费相同的**输入事实**（fixture）；几何/像素/回调预期来自 Web 观察和独立合同，不把 native 布局输出灌进 fixture。记录 `effective_inputs`，先比较两侧实际公开参数/数据与 fixture，条件不同报 setup 失败。

源码研究已读取以下固定 official 文件并计算 SHA256，原始文件只在 ignored `out/web-native-design-research/`；未运行 Web：

| 文件 | SHA256 |
| --- | --- |
| `components/button/demo/basic.tsx` | `f970528664503dd9141474bb99ea5a19037b6a8a3f55ac7b1a1d5a09c5836f03` |
| `components/input/demo/allowClear.tsx` | `ce6051c1c7b096a7d2ef823c513ef6c0378f139d1027e1bdbe4c46a8aa960a66` |
| `components/input/demo/presuffix.tsx` | `cd9ca2c65ae3fe4105d3fa9968628345bd526e22024e1a3a78a138f183d90137` |
| `components/select/demo/basic.tsx` | `925d3e5f18ead0e61b5f8490b677d4cbe6c9ebea4f23acde12a7ad65cd1e45d8` |
| `components/select/demo/search.tsx` | `14ab4f328234f5f19f792092e8f0076721446c43119f22136d36cea80aaa6dbb` |

这些 demo 的数据/尺寸并不天然等于 Gallery：例如 native `themed_button` 默认 Small 且有局部 Theme，Select 使用 desktop/service keys，而官方 basic 使用另一组数据。因此不能直接截图相减。pilot 必须先固定等价输入和适配明细；局部 Theme 要作为实际覆盖记录，不能用全局 Dark 名称声称局部按钮已切到 Dark。

### 3. 测试专用依赖与可重建缓存

`tools/demo_reference_web/package.json`、精确 `pnpm-lock.yaml` 与 `dependencies.lock.json` 是测试源；Node/pnpm、React/ReactDOM、Playwright/Chromium revision、bundler、所有传递包的精确版本/URL/integrity/SHA256/license 必须在首个实现提交冻结，禁止范围版安装和隐式 system-first。Python 比较依赖同样锁定（Pillow、数值/JSON schema 库），使用 `out/.../venv`。central native dependency lock 继续作为 Noto/Icon 资源的唯一版本来源；Web lock 引用其身份，不复制一份独立漂移的字体版本。

bootstrap 支持显式 `BUNDLED|SYSTEM`：BUNDLED 使用已校验工具归档/缓存，SYSTEM 只接受与 lock 精确相符的已安装版本；缺版本即失败。安装工作目录和 node_modules 全在 `out/`，用 `pnpm install --frozen-lockfile`；不改根 package，不为产品链接 Node，不自动修改 CMake dependency mode。实施时记录所选精确版本是必要可验证产物，此设计不凭空填写未验证的软件归档 hash。

cache key 是 reference commit、依赖 lock、Web adapter/fixture/schema/profile、font hashes、viewport/DPR/theme/locale/direction、浏览器 revision/OS/图形模式/色彩配置的摘要，**不含 native 输出图像**。reference 采用内容寻址只读目录及原子 publish；命中时验证完整清单与文件 hash，缺图/截断不能命中。默认优先用缓存，但重跑 Web 行为与动作 receipt；`--reference rebuild` 可独立重建，clean-cache CI 验证可重复安装和输出稳定区间。

### 4. 文件协议与一条命令

未来入口示例（不是本轮执行记录）：

```text
python -B tools/run_demo_differential.py inventory --binary out/build/linux-gcc/examples/Release/rynui_token_gallery --output out/demo-differential/inventory
python -B tools/run_demo_differential.py run --suite pilot --preset linux-gcc-release --binary out/build/linux-gcc/examples/Release/rynui_token_gallery --reference locked --output out/demo-differential/pilot
python -B tools/run_demo_differential.py run --case antd665.input.controlled-edit.v1 --profile linux-pinned-font --reference locked --binary <binary> --preset <preset> --output <fresh-out-directory>
python -B tools/run_demo_differential.py replay --failure <out/run/report.json> --affected --binary <fixed-binary> --preset <preset> --output <fresh-out-directory>
python -B tools/run_demo_differential.py meta-test --suite pilot --binary <binary> --preset <preset> --output <fresh-out-directory>
python -B tools/run_demo_differential.py run --suite all --coverage required --binary <binary> --preset <preset> --output <fresh-out-directory>
```

`run` 自动准备 reference，无需先手动开浏览器/选页。支持 `--component`、`--family`、重复 `--case` 和 `--changed-files <file>`，选择集排序稳定，空选择为错误；`list` 是不执行的独立子命令，不返回执行 pass。新 runner 的 `--preset` 接收 build preset，并从 CMakePresets 解析 configure preset/configuration（例如 linux-gcc-release → linux-gcc/Release），核对二进制 hello；传给旧 page runner 时使用其 configure preset 口径，不能只相信调用者字符串。

schema 文件放 `tests/fixtures/demo-differential/{coverage,case,inventory,observation,report,profile}.schema.json`，都要求 `schema_version=1`、未知字段拒绝、有限数值、唯一有序身份和输出路径约束：

| 协议 | 必需内容 |
| --- | --- |
| case | identity/revision、source refs、native route/sample/targets、mapping/adaptation、fixture、environment/profile、actions、checkpoints、oracle predicates、适用维度、依赖路径/Token/输入家族 |
| fixture | data/keys/text、Props/slots、controlled parent policy、clock epoch/seed/timezone、viewport/ROI 容器、Theme 及局部覆盖、locale/formatLocale/direction、font chain、motion policy |
| action | 单调 step id、kind、语义 target、参数、input layer、前置状态、逻辑时间；允许 pointer move/down/up/cancel、key down/up、text commit、composition、wheel、external-props、async-complete、advance-clock、navigate、checkpoint |
| observation | run nonce/case/step、adapter identity、effective inputs、动作发送/分发/命中回执、observed state、callback stream、geometry/text/icon/token、capture revision/hash/size、资源/idle 观测 |
| report | selected/expected/completed identity sets、每层 verdict、边界/错误、环境/构建指纹、首次分歧、指标/阈值来源、产物相对路径、repro/replay argv、覆盖分母与漏项 |

原生新增 `--page-scenario=<out/input.json>` 和 `--page-observations=<out/output.jsonl>`，要求 `--page-acceptance`；与旧默认巡检互斥。runner 将经过 schema 校验的 case 实例写入 out，启动同一个 `rynui_token_gallery`，不让 C++ 端评估全部 JSON oracle。C++ scenario reader 使用有限 typed grammar，拒绝任意 JS/代码执行和越界路径；旧 CLI/JSON schema 保持兼容。写入一条 `hello`（binary/source/registry 身份），每步 receipt/observation，最后 `complete` 与完整身份集/计数；截断 JSONL、重复/错序 step、缺 complete 即失败。runner 外部进程超时覆盖原生内部 timeout。

状态机：select → validate inventory/coverage → prepare reference → preflight 两侧 → execute → validate artifacts → compare → report → cleanup。预写失败状态，成功报告原子 rename，旧 out 目录拒绝复用。退出码：0 全部选中要求通过；1 完整执行但有差异；2 配置/schema/coverage/reference drift；3 环境/安装失败；4 超时/崩溃/不完整；多种失败按 4、3、2、1 优先，逐 case 仍保留全部原因。native-only、web-only 边界计数分列，非 paired pass。

### 5. native 执行只扩展现有有限验收

在 `examples/token_gallery/` 增加内部 scenario/observer 小模块，经现有 `GalleryPageEvents` 与 `GalleryPageSubmitter` 调用。复用 `GalleryPagesHost` 的 page/sample factory、target registry、dispatch、`FrameRequestState`/`DirtyQueues`、`acceptance_time`、`OnDemandFrameLoop` 和 `SceneResources → SdlSceneRenderer`。不在 Core include Web/JSON/SDL test adapter，不造测试专用绘图组件。

输入分三层标注：

- `native-core-dispatch`：直接经 `pages.dispatch` 的真实生产 focus/hit-test/popup/editor 路由，以及 `inject_scroll_at` 的文档/嵌套滚轮入口。pointer 坐标由语义 target 的观察 bounds 算出；需要通过命中记录证明落到预期对象，不能直接调用 onClick 或 set-value。
- `native-sdl-mapped`：适用平台 canary 将带 fixture timestamp 的 SDL 事件交给 `PlatformState` 的映射，再走同一路由；沿用现有 filter，补全 text/composition 隔离时不能放入用户真实事件。验证坐标/按键映射，不使用 OS 物理输入。
- `native-composition-seam`：使用真实 editor session stamp 注入 composition/commit/cancel，验证 Core 行为；不宣称 OS 输入法。

`external-props` 只模拟受控父应用本来可做的 prop 更新，清楚区别输入动作。测试观察 hooks 在实际 callback 包装和状态 snapshot 之后记录 accepted/rejected proposal；不能通过修改最终 value 模拟用户输入。Tab/Escape/focus case 不得使用旧 helper 的 `request_focus` 偷过焦点路径；初始化 focus seam 可用但必须标明 setup，并另验真实导航。

native 目前缺少结构化任意 scenario、完整 callback/event JSONL、跨组件只读 semantic snapshot、与 source writer 一致的无窗口 inventory、可声明的 sample 输入参数/父状态策略、字体策略导出和通用比较层；这些是实现任务，不假定已有。必要 sample 参数只注入现有 builder 的共享公开 Props/父状态，保留默认 Gallery 行为；不得复制一个更简单组件到测试里。观测代码保存 source mapping，不能为“需要测”而在所有生产组件增加 public test API。

每个 checkpoint：排空本次动作因果产生的同步更新，推进规定逻辑时间，要求 layout 和 scene revision 稳定，经过共同上传/attach，`save_frame_bmp` fence 完成后记录当前 run/step/revision。不能用 `submitted_frames>0` 替代当前检查点新图。hidden readback 不做 compositor callback/present 声明；现有正常帧泵 acceptance 继续单独验证。

不变状态允许 scene revision 与前一步相同，但必须有本次新 capture nonce/receipt 与 fence 完成；不能要求每次观察都制造 dirty。`idle_after` 是无 capture 的 passive observation 区间，测试自发请求的 readback 不计生产自主提交；先结束该区间再作显式诊断捕获，以免 observer 本身破坏 idle 或忽略实际持续唤醒。

Linux runner **复用** `run_background_wayland.py`：私有 runtime、Wayland socket、DBus、进程组及超时清理，绝不桌面 fallback。复用 `521a3e5e` 已冻结的 `ExistingDataEntryActions`/Other helper 接入方式，由实施基线吸收该 owner 提交后作最小桥接；本规划不导入提交或改变 base。109 adapter 不复制帧泵或跨写 WIP。Windows 用实际 MSVC 隐藏窗口、D3D12/DXIL 和无崩溃弹窗策略，平台错误单独报告。

### 6. Web 执行器与动作同构

Playwright 启动独立 headless Chromium、新 context/固定 viewport、deviceScaleFactor/locale/timezone/reduced-motion；参考服务器仅 loopback 随机端口、生命周期归 runner。临时用户目录/浏览器安装/日志在 out，不连接现有用户 tab。执行阶段路由拦截外网请求，未声明网络访问失败，不偷偷拿线上图标/字体。

稳定 target mapping 使用 reference wrapper 的 `data-testid`、官方公开 role/label 与受版本约束的部件 locator；index-only locator 必须绑定稳定 key 和集合检查。观察 `getBoundingClientRect`、computed style/pseudo style、DOM Range text boxes、可见文本/selection、DOM focus、callback wrapper 与 popup 边界，不把 CSS 声明 alone 当成已绘制像素。

点击/键盘/滚轮通过浏览器输入 API；不能用 DOM `.click()` 或直接改 input.value 代替真实输入路径。合成 composition 通过显式 browser protocol/seam，记录其级别与独立 expected，DOM synthetic event 不宣称真实键盘/IME。禁止“Web 收到什么事件，就强制 native 也产生同名 DOM 事件”；只比较可对应的语义动作/状态。

disabled/loading 的拒绝输入 case 不使用会等待 enabled 的 `locator.click()`；在已核对的控件 rect 上执行浏览器 mouse down/up，并记录发送回执，再断言 callback 为零。hidden 负例显式使用隐藏前保存的位置或背景 target，不能把“找不到隐藏目标，所以没发送”算通过。键盘负例同样先记录实际 focused target 和发出的键，再判断组件是否消费。

`advance-clock` 在页面加载前安装 Playwright clock，按需推进 timers/RAF；CSS transition/animation 需要另通过 Web Animations/浏览器动画控制接口设置同一逻辑时点并等待 paint，不能假设 JS clock 自动冻结所有 CSS。记录 animation identity、start/currentTime/playState，跑 timeout/transitionend/反向 meta canary；不能控制或观察到实际动画时点时报 unsupported sampling **失败**，不退化到 wall sleep 后声称确定时间。`screenshot` 不使用自动 finish animations；静态 motion-disabled case 与动态 case 分开。

技术依据为 [Playwright Clock](https://playwright.dev/docs/clock) 的 clock 范围；实现所选锁定版本还须通过对应 adapter canary，不能以当前文档能力替代已安装版本验证。

### 7. ROI、坐标、DPR 与可见性

同一 checkpoint 包含 `sample-context`（整个声明样例容器）、每个语义 target/部件的 `control` ROI、popup/window-layer ROI 和 clip 外 `guard-band`。匹配依据显式 target/key，不靠截图图像搜索、最近矩形或本地化文本猜测。参考与 native 分别观察几何，再转换为 sample/viewport 坐标；只允许减去**已声明容器原点**的刚性平移，保留子元素的真实偏移。不能把两个控件各自裁到紧边界而抹掉位置误差。

Gallery 的导航/说明卡/源码折叠壳没有官方 demo 同构，单列 native-only shell 合同；paired ROI 包含样例实际内容及其 slot/邻接布局，不把外层不同的文档壳混入像素分母。适配容器的宽度、padding、布局策略必须来自 fixture 与声明设计合同，不能从 native measured bounds 复制。删除成对 slot、只比单控件而跳过原有组合布局会触发 coverage/context gate。

CSS px 对应 native logical unit；两侧使用同一 effective DPR 和逻辑容器宽高。先比较逻辑 rect/clip/baseline/scroll extent；像素捕获按共同预期 ROI 的 floor(left/top × DPR)、ceil(right/bottom × DPR)，并取 reference 与 native bounds 的并集加 guard，防止 native 自报过小尺寸掩盖漏绘。原图与完整坐标保留；crop 尺寸不符是错误，不 resize、不非刚性配准、不用最佳平移搜索消除错位。

默认 pilot `--dpr=native`：先从实际 native window metrics 读取 scale/physical extent，再令 Web 模拟相同 DPR，校验最终逻辑/物理尺寸。显式 `--dpr=<value>` 要求 native 环境实际匹配，否则 environment error。现有 `read_frame_rgba` 要求 composition 与 window metrics 相同，不能只改记录的 DPR 或偷偷缩放 BMP。小数 DPR 的通用数学/calibration 测试可使用 1/1.25/1.5/2；实际平台 GPU 覆盖只报告该平台真实运行值，其他值不算通过。增加受控 native capture metrics 若确有必要须扩展既有 metrics/readback 合同，不能在本 change 私造 framebuffer。

滚动/长样例用声明的语义锚点与重叠 tiles，记录已覆盖 logical intervals，自动检查全区间无洞；popup 归属于原 sample 但使用 window 坐标，不能被文档 clip 裁掉。guard 带宽至少包含预期阴影/outline support 再加 2 physical px；用 reference 占用集合排除合法邻居，检查原始 context 的新增/丢失连通区域，防止“只比漂亮中心”或从 ROI 外漏出的错误被忽略。

### 8. 分层比较器与硬门槛

一例通过是所有适用检查的 AND，不是加权平均分。每层产生独立 metric、expected/observed、阈值/profile 来源与首次失败位置；unsupported 指标不能静默降级。下列为实现的初始精度目标和**最大容差预算**，须经第 9 节的自动校准获得实际更紧阈值；本规划未证明它们在所有环境可达。

| 层 | 比较与判定 |
| --- | --- |
| 条件/语义 | case/输入/文本/keys/选项顺序/可见性/状态/图标来源/事件精确相等；浮点业务值按 case 的领域精度，不全局 round。NFC 等归一化只有明确合同允许时采用，不能吞编辑文本差异。 |
| 几何 | 比较容器、content、padding、gap、尺寸、对齐、基线、clip、border/radius、scroll viewport/extent、popup placement。非文字几何初始误差 ≤0.25 logical px，上限不得吞掉 1 logical px 的位移/尺寸故障；行盒/字形边界另有字体 profile，默认上限 0.5 logical px。比较相对父/兄弟的间距和绝对 sample 位置，不能只比尺寸。 |
| Token/样式 | Web computed style + 固定官方 token 输入，对应 native effective Theme/component token、实际 primitive 材质；纯色、border width/style/radius、opacity、shadow offset/spread/blur 与 icon 名称/hash 逐项判定。语义映射写明单位转换和 component recipe 来源，不能把不同配方仅因当前像素接近当相同。 |
| 实际填充/颜色 | 在参考和 native 几何共同内缩的平坦区域比较色值，同时检查区域面积和边界，不因区域消失而忽略。经色彩校准后的实色 ΔE00 最大预算 1.0，编码 byte residual 同时报告；gradient 按方向/停点/硬断点分块检查，不用均值掩盖色带。 |
| 边框/圆角/图标 | 非文本 ROI 的双向 contour distance、occupancy/connected components、局部像素 residual；初始边缘距离预算 ≤0.5 physical px，AA 带只采用 coverage 指标而非排除。图标每个部件需有足够非背景像素，匹配离线 vector identity/path hash/方向和大小，再检查 raster，丢一个小图标独立失败。 |
| 文字 | 内容/行数/line breaks、run direction、font face/size/weight、基线/advance/ink bounds/clip 必验；每行及每 glyph/run 小 ROI 检查墨迹存在、coverage mass、投影和双向轮廓，允许经校准的灰度 AA 差异但不允许整行 mask。每 glyph 墨迹 mass 预算差 ≤5%、轮廓位移 ≤1 physical px；高对比内部仍比较颜色。该预算不能接纳字体替换、裁字或错字号故障。 |
| 阴影/半透明 | offset/spread/blur/radius/opacity 数值先验，加沿各边的亮度/alpha 剖面、support extent、局部残差；分别在两个已知背景上采样半透明校准块，验证 premultiplied/source-over。不能用整图 SSIM 掩盖叠加/漏覆盖。 |
| 上下文/overflow | reference/native 占用差的连通块、邻接关系、guard 带泄漏，保留真实 clip 外空白预期；任何必需前景缺失独立失败。 |

可报告 SSIM/多尺度灰度距离作为定位排序，但不得独自裁决。边缘 AA 允许“宽度不超过 1 physical px 的已知 raster transition 带”，其自身仍需轮廓/coverage 通过；不是容忍任意邻域 blur。文字体量大的 ROI 按 run/glyph 分解，防止多数正常字吞掉少量缺字。无法从 native 当前观测得到 font/glyph/clip 数据就是观测接口缺失，不能只靠像素相似率判绿。

色彩处理不假设 RynUI 已线性化：保留原始 RGBA8/BGRA8、实际 framebuffer format、channel order、alpha、清屏背景和色彩 profile。Web 固定 SDR/sRGB 输出，解码到标准观察色域计算 ΔE/亮度；若 native 返回编码值来源不明，先跑独立实色/alpha/ramp 校准。转换必须预先声明并双侧一致，不对失败图做 histogram matching、调 gamma 或颜色“修复”。renderer 当前渐变/alpha 编码合同与浏览器不同产生的真实差异要报出，由适用合同决定修复或具名非等价边界，不靠任意容差掩盖；Progress 数值工作仍由其 owner 负责。

### 9. 字体策略与容差校准

提供两个明确 profile：

- `*-pinned-font` 是自动视觉主门槛，显式使用仓库锁定的 Noto Sans 2.008、Noto Sans CJK SC 2.004，以及需要时既有 Arabic/Hebrew 锁定字体与 Icon 资源；Web `@font-face`/font loading 检查、native `DefaultFontChainRequest.preferred_fonts` 共用相同 bytes/face index/weight。缺字不得悄悄落到系统字体；Latin/CJK/混合 bidi corpus 检查实际 selected face 与 fallback。此模式只在验收入口启用，不能替换普通 Gallery 系统字体默认值，也不伪造 `system_fonts=true` 满足旧 schema。
- `*-system-font` 用现有 OS 字体链，导出实际 face/hash/weight/style/字号供 Web 同机加载同一 face（仅本地、受 license 约束、不上传）。字体无法按同一条件对齐则 environment mismatch；至少继续独立原生系统字体合同，但不得将 paired 视觉标 pass。Windows/Linux 结果分别保存，禁止跨 OS 复用图像基线。

字体观测不能只相信 computed `font-family`：Web 结合受控字体请求/hash、font loading 完成、锁定 Chromium 的 actual platform-font 查询和文本度量；native 导出实际 face/run。粗体/斜体 case 必须冻结对应 face 或显式、双侧一致的 synthesis policy，不能用 regular lock 加静默系统回退冒充相同字体。缺少匹配 face 时准备失败，必要新增字体归入同一资源锁。DOM 不公开 shaping glyph ID，不强行与 native glyph 编号相等；以 grapheme/ligature group 的 Range、行盒、文字 raster 和实际 font runs 对应，字符索引转换独立测试。

新增 observation schema 允许声明 pinned/system 两模式；旧 page acceptance 的系统字体要求保持不变。Theme locale 与 direction 分开：zh-CN/en-US 及 formatLocale 的实际 fallback、LTR/RTL、业务 literal 与翻译 key 均由 fixture 声明，不能通过把全部文案换成英文消除裁切问题。

校准使用确定性输入 corpus（纯色、1px border、圆角、阴影、clip、正式图标、Latin/CJK/bidi 文本、透明叠加），由两侧真实组件/renderer 执行；源码程序是测试代码，运行图不提交为 fixture。选取不属于待测组件差异的已知几何/字体/配色控制样本，并用解析几何/固定输入独立断言控制样本本身正确。

每环境正控制重复至少 5 次，以相同 source/adapter 的重复输出测噪声；Web/native 控制对照估计允许的 raster 残差，但必须同时通过几何/内容等严格层。每 metric 计算 `candidate = max(quantization_floor, repeated_positive_max + fixed_margin)`，同时在留出 corpus 与最小负控制运行。要求 `candidate <= hard_cap` 且每个负控制的失败层有安全间隔；不满足则 calibration error。不得将当前待判 native-vs-Web 的残差或最近失败样本纳入正控制。

profile 记录 corpus/算法/工具/source/environment hashes、重复数、正负控制区间、具体阈值及 hard cap；runtime 只使用匹配环境的有效 profile。容差更新是独立工具变更，必须跑完整 meta tests 和影响集，不自动从红图学习。pilot masks 默认为空；后续只允许具名非合同装饰背景、上限为 ROI 的 1%，并禁止与文字、图标、border、clip/guard、状态标记或动作目标交叠。动态 caret/时间内容通过固定时点/fixture 控制，不能整块屏蔽编辑器。

### 10. 独立语义 oracle、事件与生命周期

`case.oracle` 是受 schema 限定的声明性断言集合，由 pinned Web/API 合同和 RynUI 公开合同编写，不由任一 observed output 生成：`equals`、`unchanged`、`count`、`ordered_events`、`happens_before`、`focused`、`contains_keys_in_order`、`within`、`consumed_by`、`retired`、`idle_after` 等。对两侧分别验 oracle，再比较明确可对应的 projected trace；不强制比较框架内部 DOM events 与 C++ callbacks 的数量。

trace 行保存 `seq,step,time_us,target,key,event,payload,owner_generation,layer`。DOM random id、指针值、机器时间可归一化成稳定语义名；事件重复、值、目标、接受/拒绝、先后关系不能删。异步无因果的事件用显式 partial order；同一 callback 流保持严格顺序。Web React StrictMode 在 reference profile 中显式关闭并记录，不能事后把多余 callback 去重。

| 合同族 | 规范化输出与 oracle |
| --- | --- |
| controlled | 区分 proposed value、父应用 accept/reject、committed value 与可见 draft；reject 不推进受控值，callback 次数/负载符合组件自己的合同。例 Input 的编辑 draft 行为不能套用 Select 选择行为。 |
| focus/input | Tab/Shift+Tab 顺序、focus modality、Escape 层级、disabled/loading eligibility；每动作确实分发，观察实际 focus/capture/命中，而非 setup request_focus 的结果。 |
| popup/childModal | active stack、背景不可交互、父/子弹层关闭与 focus restore；按声明关系比较，不比较 OS window handle。 |
| scroll | 容器/子控件消费、offset/extent/边界，滚动 delta 以相同 logical distance 表达；Web/native wheel 单位转换写入 adapter 并测试，不靠各自“滚到底”掩盖 delta 错误。 |
| 编辑/IME seam | 文字、grapheme/UTF-8 与 DOM UTF-16 选区显式转换、选区方向、preedit/commit/cancel、过期 session；组合事件仅为合成证据。 |
| 动态拓扑 | key 顺序、节点存续/重建、草稿/选区保留、删除后的事件拒绝；identity 映射只暴露 stable key 和本 run generation，不比较地址。 |
| 异步/时间 | fixture 控制 Promise/transport 完成顺序、取消/late completion、timeout/debounce、动画起点/中间/终点/反向，不能真实网络随机完成。 |
| hidden/离页/idle | 非活动页 component runs、owner-local callback/capture/session/deadline 清零；保留合法 shell owner 与字体共享缓存；稳定后两次无动作 observation 无新 upload/submission，持续动画先 hidden/unmount 再测 idle。 |

ConfigProvider inheritance matrix 必须**逐组件逐字段**读取 pinned contract。具体负例：Progress 的官方 Circle/Line 与当前 native 映射仅消费 Provider direction，不继承 Provider size/disabled；它自己的 size/disabled 与显式 LTR 优先规则独立测试。改变 Provider size/disabled 时要求其相应输出不变，不能用“全体组件自动继承”生成 expected；对真正消费这些字段的控件另验变化。该条来自协调交接并需在映射实现时锁定具体官方文件 hash，不涉及修改 Progress 代码或数值 oracle。

例如声明性 oracle 可以写 `{"op":"count","stream":"callbacks","step":"click-disabled","target":"disabled","event":"activate","equals":0}`，并另写 `{"op":"equals","path":"receipts.click-disabled.dispatched","value":true}`；两个断言必须同时满足，不能把“没执行所以零 callback”当绿。数值/geometry oracle 的 `expected_from:"web.observation.<field>"` 与独立常量/关系明确区分，禁止 `expected_from:"native.*"`。grammar 不支持任意可执行表达式；新 predicate 需带正负控制和 schema 版本。

默认动画取样为 t=0、duration/4、duration/2、3duration/4、duration/end+settle，另有中途 retarget/reverse。duration/easing 从锁定 case 合同和 reference 实测动画 metadata 核对，native 使用 existing controlled clock；不同端的启动提交点绑定同一个语义 action，不让 reference 晚一帧而靠扩大像素阈值补偿。每个 sample 同时验证预期几何/opacity progression 和实际画面，动画终点通过不能抵消中段失败。

### 11. 最小 pilot 先于全目录图像扩展

首先打通以下真实注册样例，默认固定字体、DPR=实际 native、motion-disabled 稳态；然后加入确定时间动态检查。每个 case 均有 official provenance 和本地 target，不能仅创建 mock 图证明比较器能运算。

| 首批 case | native 入口 | 固定 Web 来源与首个 oracle |
| --- | --- | --- |
| `antd665.button.basic-states.v1` | `gallery.sample.button.basics`，`gallery.state.default/primary/disabled/loading` | basic demo + 对应 button source，按实际 Small/局部 Theme/标签参数化；hover/down/up/Tab、disabled/loading 不错误触发，检查 padding/border/radius/图标与实际 pixels。callback 观测需补到实际样例 handler，不伪造计数。 |
| `antd665.input.controlled-edit.v1` | `gallery.sample.input.basics`，`gallery.input.controlled/uncontrolled` | allowClear/presuffix + 输入组件源码，固定前后缀、maxLength、文本/宽度；真实键盘/commit、父 accept/reject、选区/clear、draft 与 callback。现有 builder 未暴露 reject policy，新增内部 sample 参数，不新建替代 Input。 |
| `antd665.select.popup-lifecycle.v1` | `gallery.sample.select.options`，`gallery.select.controlled-search/disabled/loading/controlled-open` | basic/search + Select 源码，固定 desktop/service/unavailable keys、宽度、controlled open；pointer open、Arrow/Enter/Escape、拒绝值/关闭、popup clip/焦点/disabled、离页清理。 |

先跑每例 1 个 zh-CN/LTR/default variant，必须真实完成 capture→compare→report→red/green；成功后加 en-US/RTL、Dark/Compact、窄宽与三类代表动画；不先抓完全部目录图等人看。普通 Editor/Select 受控 policy 不相同，独立 oracle 以其 pinned 合同为准，必要 native-only 行为显式分层。

pilot 不是特设总范围：后续每个当前 sample 都登记 case/适用维度，推进输入五家族、布局/图标/文本、popup/导航、数据/表单/媒体/Other。既有 native-only 场景也纳入账本；当前组件与官方不存在一比一 sample 不得整体跳过，要做公开 Props 的具名适配。全覆盖报告给出每个 canonical/家族/sample/target/feature 的已执行及缺口，不把 theme×width×state 截图张数当合同数量。

### 12. meta tests：必须能自动红再绿

分三层验证，缺一不能完成 pilot：

1. **比较器单元负例**：确定性程序生成图形/几何/trace 输入，测试 pixel/contour/clip/text/顺序规则及阈值边界，输出只在 out。只证明算法本身，不代替真实集成。
2. **adapter/runner 协议负例**：空 capture、全透明/纯背景、同一旧帧冒充新 checkpoint、错 source/hash/version、缺 case/target、同 count 换身份、重复/乱序回执、crash/timeout/truncated JSON、动作未执行、context/scale/font 不匹配、全局阈值/过宽 mask、错 expected/oracle 缺字段、路径逃逸。每种应得到指定非零原因，不将它们变 skip。
3. **真实路径故障注入**：BUILD_TESTING/acceptance-only hooks 或隔离测试构建，在真实 sample Props/Theme/callback 包装/实际 scene 发布点注入；不修改报告/已保存图片。正常 Gallery 不暴露该开关，fault id 写入 fingerprint，faulted binary/run 禁止 reference cache publish。

首批要求 visual 与 logic 至少各一个真故障，扩展阶段完成如下最小集合：

| 注入 | 预期检出 | 恢复验证 |
| --- | --- | --- |
| Button padding +1 logical px / 纯色偏移 ΔE≥3 / radius +2 logical px | geometry/token/局部 pixels；背景很大也必须红 | 去 fault 后同 case、profile、参考命令绿 |
| Input 末字裁去 1 logical px / 换错字体或字号 +1 | clip/文字行盒/墨迹门槛；metadata 仍正确但实际少画的变体也需检出 | 同上 |
| 删除正式图标的真实 scene primitive | icon ink/occupancy/pixels；即使 semantic icon 名称仍正确也红 | 同上 |
| 受控 reject 时错误提交 / 重复或丢失 callback | 独立 oracle + trace 次数/负载/顺序 | 同上 |
| disabled 错误响应 / Tab 被跳过或焦点落错对象 | input receipt + state/focus oracle | 同上 |
| 错 clip、错误 alpha 或失效后仍有 deadline | guard/color/lifecycle | 同上 |

故障样本不得用于调宽阈值；schema/meta-test 对故意把 expected 从 native 实测生成的 observer 给出不可信来源失败。额外“双侧都错且 trace 相等”负例证明独立 oracle 有效。Progress Provider size/disabled 的不消费负控制与 Button 等真实消费者的正控制配对，防机制本身生成伪回归。

`meta-test` 总结果要求：无 fault 正控制绿 → 每个 fault 子运行红且故障层正确 → 无 fault 复跑绿；任何某 fault 未检出都使 meta-test 自身非零。注入运行的 `expected_failure=true` 只属于 meta 外层，普通 run 的 faulted case 永远不是 pass。

### 13. 报告、指纹与修复回归

目录布局：`out/demo-differential/<run-id>/{request.json,inventory.json,provenance.json,report.json,junit.xml,index.html,cases/<case>/<variant>/<checkpoint>/{web.png,native.png,diff.png,geometry.json,trace.jsonl},logs/}`。cache 单独在 `out/demo-differential/cache/`，按 hash 重建。仓库只提交短 Markdown 结论、schema、声明性输入/lock 和可复用代码；不能把报告/截图/逐样本日志改名 fixtures。

provenance 包括 source HEAD、working diff hash、相关 source/generated/lock/adapter/oracle hashes、binary SHA256、嵌入的 build manifest、preset/config/toolchain、renderer/shader hash/capabilities、GPU/driver、compositor executable/加载库 hash、OS/browser/headless/GL mode、font faces、actual metrics、color/readback/fence 条件。仅 `git rev-parse HEAD` 不足证明 dirty build 来源；构建侧生成相关输入 closure manifest 并嵌入 hash，runner 对照二进制 hello，源码变化但未重建即拒绝。cache 不跨不兼容环境复用；软件 Web raster 可声明为 Web-only render profile，但 native Vulkan/D3D12 不得被 RECORDING 或软件替代冒充 GPU 集成。

自动分类优先输出观察事实：geometry→layout，text/font/ink→typography，effective Token→token，状态差→state，缺分发/命中→input，callback/value order→logic，retired/deadline→lifecycle，像素残差→visual；前置来源/环境失败单列。可同时有多类，关联 component source/Theme token/renderer 文件仅为 `suspected_owner`，未经实证不能写确定根因。

report 保存首次分歧 step、最小失败 ROI 和上下文、两侧 observed/expected、阈值、原始 trace与归一化 trace、声明的 repro argv；`replay --failure ... --affected` 固定旧 case/参考/profile，检测新二进制/adapter/source，自动选择失败集加依赖闭包并生成全新 run。若 fixture/oracle 本身被改，标为测试定义迁移，必须连同旧合同回归与 meta tests，不能伪称同条件修复成功。

正常修复由 Sol 根据报告定位/改代码/复跑，机器结果才决定通过。普通问题不等待 root 派设计 review；确有未解决机制、数值或架构冲突时再由 root 决定 NEW Astra 工作。reference migration 是独立命令/提交，禁止 `--update-snapshots-from-native` 或 native fail 自动重建 expected；更新参考版本须更新 source lock、映射、profile/meta tests 以及受影响范围，不与普通修复混合。

### 14. 选择策略、CI、预算与平台分离

每个 case 依赖记录 source function/public headers、组件实现、Theme domains、font/renderer/input 家族和 adapter/oracle/profile。`--changed-files` 来自明确基线到目标 revision 的差量与 working changes；不 fetch/rebase，不猜未提供的旧基线。普通组件改动选依赖 case 与复合消费者；Input/Focus/Popup/Theme/font/renderer 选家族全适用集和跨组件哨兵；schema/comparator/source lock/unknown shared change 选全适用集合。选择器本身 meta tests 覆盖依赖漏边、rename/delete、新 case、same-count 换身份、未知路径保守扩大。

频率：每次局部修改先 targeted，PR/集成 CI 跑 pilot+机制负例+影响集；显式 full job/既有发布 CI 跑当前完整 baseline 集合和从零 cache 构建。可以为仓库已有 CI 设计周期间隔，但本 change 不创建用户调度器、监控或定时任务，也不在每小修后跑全 CTest/GUI 矩阵。每个阶段独立验证/提交，最后收口只汇总已完成平台结果。

预算是实现起始约束，后续实测填写，不冒称性能已达标：单 native GPU worker、单 browser worker，比较器最多 2 CPU jobs；冷准备预算 10 分钟，warm pilot 目标 5 分钟内、targeted 10 分钟内、full shard 30 分钟；单 case 默认 30 秒/最多 120 秒、动作最多 4096/检查点最多 256（都是防失控上限，可显式审计扩展，**不是 inventory 总量上限**）。超时/磁盘预算耗尽记录余下 not-run 并非零；禁止截掉 case 让 CI 变绿。报告 wall/CPU、峰值 RSS、artifact bytes、各阶段耗时和 cache hit，冷/热分别统计，单次诊断重跑不能覆盖首次失败。

平台通用：Python schema/选择/比较/负例、C++ scenario/观测/输入合同在一个支持平台的 HEADLESS Debug/Release 验收；按实际使用 `linux-gcc-headless` 或 `windows-msvc-headless` 记录，不要求两次验证同一逻辑。生产边界仍经过 include/link guards，禁止 Web test deps 进入 Core。

Linux：正式 `linux-gcc`/`linux-gcc-release`、Ninja Multi-Config、实际 Vulkan/SPIR-V、私有 Wayland/DBus、pinned/system fonts 分开；记录实际 compositor/Clutter 路径/哈希，不在任务中修改系统 compositor。Windows：实际 Windows、`windows-msvc`/`windows-msvc-release`、MSVC/D3D12/DXIL、系统字体/SDL 输入映射和隐藏读回，独立 checkbox。Windows 测试平台相关 target 的 Debug/Release；MinGW/Wine/Linux 结果不得代替。平台准备或图形栈不可用保留失败状态，不回退桌面。

### 15. Owner 与共享 hunk 边界

109 新机制主要位于新 `tools/demo_differential/`、`tools/demo_reference_web/`、`tests/fixtures/demo-differential/` 和小型 Gallery 内部 adapter 文件；主协调者只协调接口/提交顺序，Sol 实施。`gallery_pages_runtime.cpp`、`token_gallery_definition.cpp`、`gallery_test_inventory.cpp`、CMake 登记、目录/Theme 由当前 owner 先冻结相关改动；109 只作最小 include/call/参数 hook，不整体搬移旧 acceptance 巨函数或批量重写样例。

Progress 数值分支可贡献明确冻结的独立 oracle/negative case，109 不读取或改写其未冻结 source、cap 实现或故障产物。Listy/Table 与 Other 的新样例由实际 writer 自动进入缺口报告，不能为了维持旧 count 删除。实施者交付记录 base SHA、精确增量身份/顺序、局部命令结果与未测边界；本设计分支只做本地 Conventional Commit，无 upstream、push、fetch、rebase、reset、PR 或 archive。

## Risks / Trade-offs

- [当前 sample 与官方 demo 不同] → 原样 demo 和具名参数化参考并列；先验证相同输入，禁止用 native 实测尺寸反推参考。逐 sample 增加 adapter 有成本，但能避免错误指控和虚假通过。
- [文字/AA/渐变跨 renderer 不能普遍像素相等] → 固定字体、逻辑几何/语义硬门槛与局部 coverage 分层；阈值不可分离正负控制时报校准失败。不能保证任意字体/任意 OS 的视觉等价，也不依赖人工放行。
- [独立 oracle 写错] → official source/trace + 声明合同双重验证、组件消费 matrix、双侧同错负例、实际故障注入；不自动从 native 输出生成预期。
- [浏览器 clock 不控制全部 CSS] → CSS/JS 两种时钟探针、实际动画 metadata/paint receipt；无法精确采样时明确失败，不用 wall sleep 伪证。
- [现有 native-only 生命周期没有 Web 同构] → 保留单侧合同和正交 coverage 类别，不伪造 DOM owner 与 native Scene 的一一对应。
- [全目录组合爆炸] → feature-driven 代表集、pairwise 基础 variant 与必须的极值/交互 case，声明未覆盖组合；all 只代表当前声明 obligations 全执行，不宣称所有可能 Props 的数学等价。
- [系统 IME/AT、驱动实际 present、跨物理显示器] → 本机制未提供这些证据；使用已有独立平台验收结果时按来源引用，不新增人工 checkbox 或把其作为自动差分的放行门槛。

## Migration Plan

按 tasks 的依赖顺序落地：协议/inventory → 固定 Web 与 native adapter → 三例真实 pilot/故障红绿 → 比较层与完整语义族 → 全目录 obligations → Linux/Windows 独立验收与 CI。每阶段可以独立提交，任何未完成范围保留 pending，不提前勾选。

旧 `run_gallery_page_acceptance.py` 和 capture-only 工具保持可用；新 runner 复用其 isolation/finite native 接口并以新 schema 输出机器判定。旧 capture report 不迁移成 baseline pass，历史截图不自动当黄金图。回滚某阶段时保留旧入口与新失败记录，移除新机制的 CI required 标记必须对应明确未完成状态，不能将缺失系统表述为全绿。

本次规划仅运行源码 inventory/已有 metadata checker、读取固定官方源文件与 OpenSpec 校验；未执行任何 pilot、校准、故障注入或平台运行。实现后的精确依赖版本、校准数值、case 数量与平台时间由锁定输入和自动结果填写；这不会改变上述门槛，也不授权悄悄缩减覆盖。
