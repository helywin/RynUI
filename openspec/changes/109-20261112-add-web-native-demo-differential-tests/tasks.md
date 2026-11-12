# Tasks

规划已进入 apply；完成状态仅以各项实现与自动验证为准。每阶段通过相应自动验证后可独立 Conventional Commit；不能以采集成功、source review 或本规划通过来勾选。阶段 5 的真实三例 pilot 必须先于阶段 6–9 的全量扩展，遇到真实 native 差异先修复并按同参考复跑，不能放宽 oracle。平台通用任务只在一个受支持平台记录一次；Linux 和 Windows 集成项分别完成、分别提交。

下列为待实现命令/target，详细 CLI/schema/退出码见 design。`PYTEST` 指 `python -B -m unittest discover -s tests -p 'demo_differential_*_tests.py'`，允许按文件缩小到本阶段。`CORE` 指正式 `linux-gcc-headless` 或 `windows-msvc-headless` 配置，构建相关 `rynui_demo_differential_tests` 等 target，Debug/Release 定向运行 `ctest --preset <headless-build-preset> --no-tests=error -R '^rynui\.demo_differential_'`。实际所用 OS/preset 必须写入短结论。每阶段文档运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、`git diff --check`；C++/HLSL 修改另运行 clang-format 22.x 的 `python scripts/format-code.py --check`。不为本 change 重跑无关 105/106 平台余项或全部 CTest。

## 1. 平台通用：协议、实际 inventory 与覆盖守卫

- [x] 1.1 建立 case/coverage/inventory/observation/report/profile schema v1 和有限 action/oracle grammar；通过 PYTEST 的未知字段、重复身份、非有限数值、非法路径及缺 oracle 正负例。
- [x] 1.2 实现 source writer/catalog/route/sample/target 的完整有序枚举和本地基线差量，保留 owner/alias；通过新增、删除、同 count 换身份、重排、alias 循环及旧身份保留测试，不硬编码历史总量上限。
- [x] 1.3 在现有 Gallery finite CLI 增加无窗口 `--page-inventory`，导出实际编译 registry/source provenance；通过 CORE 和静态 writer 逐项一致性测试，不能只比较数量或 `<40` 启发式。
- [x] 1.4 建立全部当前 canonical/家族/sample/target 的声明性 coverage ledger 骨架，区分 paired/native-only/web-only/unsupported/pending；通过 completeness 测试，未映射或 planned 不会计 pass，List/Util 只计说明页边界。
- [x] 1.5 写协议/覆盖维护文档及准确计数口径，跑 inventory 自检与文档校验；记录实施所选不可变 base、完整顺序差量及当前实际数量，保留新增 sample 的真实 source 消费检查。

## 2. 平台通用：固定官方 Web 参考与隔离依赖

- [x] 2.1 实现独立测试工具 bootstrap，冻结 Node/pnpm、React/ReactDOM、Playwright/browser、bundler、Python 库及传递依赖的版本/URL/integrity/SHA256/license；在 fresh out 下 frozen-lockfile 安装成功，版本错配/缺依赖负例非零，原生 HEADLESS 不依赖该安装。
- [x] 2.2 实现固定 commit 源码/资源准备、导入 closure/hash 校验与 BUNDLED/SYSTEM 精确模式；通过错 tag、错 source、篡改 demo/Token、cache 截断测试及现有 AntD baseline/catalog checker。
- [x] 2.3 实现 headless Web adapter，直接运行 Button/Input/Select 官方 demo export 和具名参数化 wrapper；用模块身份/effective-input 自动测试证明真实官方组件被执行、无私改 CSS 配方或 native 测量值反灌。
- [x] 2.4 实现 semantic target、浏览器真实输入、callback/geometry/text/style observation 和 complete receipts；通过 Web adapter 自动正负例检出缺 target、不执行动作、错误 props/数据及无新帧。
- [x] 2.5 实现只读内容寻址 reference cache 与重建，安装/网络/字体/浏览器参数全归档到 out；通过 fresh-cache/cache-hit 一致性、错环境拒用和 execute 阶段未声明外网请求失败测试。
- [x] 2.6 写 reference provenance/依赖安装/参数化边界文档，验证文档准备命令可执行及源代码/依赖/截图均未写到 tracked fixture；本阶段不报告 native/Web 差分通过。

## 3. 平台通用：复用 native finite 输入与观测

- [x] 3.1 增加 `--page-scenario`/`--page-observations` 有界 parser、hello/receipt/checkpoint/complete 协议，保留旧 CLI；通过参数/schema/截断/重复 step/超时路径测试和既有 gallery_page_acceptance 参数测试。
- [x] 3.2 在现有 `GalleryPageEvents`/`GalleryPageSubmitter`/`ExistingDataEntryActions` 接口桥接 scenario，复用 `pages.dispatch`、scroll 与当前帧泵；通过 CORE 输入路由测试证明不直接调用待测 onClick/set-value，无第二渲染或上传路径。
- [x] 3.3 为首批真实 sample builder 增加必要内部输入/受控父策略与 callback observer，默认行为保持；通过现有 sample/page tests 及 accept/reject/重复 callback 负例，observer 不生成 expected。
- [ ] 3.4 实现 semantic snapshot：target/key、bounds/clip/scroll、focus/capture/session、文字/字体/icon/Token、owner-local 生命周期与 scene revision；通过 CORE 校验合法 shell owner/共享缓存不会被误报泄漏，旧代际事件被拒绝。
- [ ] 3.5 在既有 GPU readback 路径导出当前 checkpoint/frame/fence/实际 metrics 指纹，并显式支持测试字体链；通过 mock/recording 协议测试检出旧帧、错 DPR、错误 font-source 标记及 source/binary provenance 不一致，不将其描述为 GPU 验收。
- [ ] 3.6 将相关 scenario/observer target 接入 CMakePresets 的正式 HEADLESS 构建并写接口文档；通过 CORE Debug/Release、旧 finite runner tests 及 include/link guards，确认 Core 无 Web/DOM/SDL/renderer 反向依赖。

## 4. 平台通用：最小可判定 runner 与 pilot 定义

- [ ] 4.1 实现 select→prepare→preflight→execute→compare→report→cleanup 状态机及 design 的 0/1/2/3/4 退出码；PYTEST 必须拒绝空选择、零退出但漏动作/图像/complete、崩溃/超时、过期 out 与路径逃逸。
- [ ] 4.2 实现 pilot 所需的 target ROI/context/guard、等比例坐标与 DPR 检查、基础 geometry/token/pixel/文字存在性判定；通过程序生成正负控制，1 logical px 偏差、小图标丢失/局部纯背景不能被大背景稀释。
- [x] 4.3 实现独立 oracle evaluator 与两侧语义 trace 归一化，声明精确 callback/状态/焦点断言；通过双侧同错、次数/顺序改变、未执行导致零 callback 和 native-derived expected 负例。
- [ ] 4.4 定义 Button basic-states、Input controlled-edit、Select popup-lifecycle 三个真实 case，绑定固定 demo/source hash、native sample/targets、完整输入/适配说明；schema/coverage/effective-input 测试全部通过，执行顺序与 checkpoints 明确。
- [ ] 4.5 实现 pilot 的 acceptance-only 真实外观/逻辑故障开关和 meta 外层判定，至少覆盖 padding +1 与错误受控提交/重复 callback；单元测试证明 faulted 普通 run 不可 pass、不发布 reference cache，正常产品路径不启用 fault。
- [ ] 4.6 输出初版 report.json/JUnit/本地 index、首次分歧和 replay argv，写单命令 pilot 文档；用 runner 故障替身验证分类/路径/退出码与 artifact 完整性，再进入实际平台 pilot。

## 5. Linux：最先完成真实三例 pilot

- [ ] 5.1 使用正式 linux-gcc-release 构建 Gallery，在私有 Wayland/DBus、隐藏窗口/Vulkan 与独立 headless browser 上运行 `run --suite pilot --reference locked`；三例必须得到完整同语义动作/几何/状态/trace/实际 GPU 图及机器 verdict，记录 compositor/GPU/font/scale 指纹与 desktop_fallback=false。
- [ ] 5.2 运行 `meta-test --suite pilot`，证明真实 padding 故障和真实逻辑故障各自非零且分类正确，去故障后同命令三例全绿；当前 native 正常路径若存在差异，修复相应实现并保留修复前后证据，不改官方 baseline 消红。
- [ ] 5.3 对 pilot 自动测空采集、错版本、漏 case、crash、跳过动作与宽松 mask；确认报告非零、进程树/私有 socket 清理、无桌面输入，记录简短 Linux pilot 结果和实际 cold/warm 耗时，原始文件仅 out。

## 6. 平台通用：完整视觉层与自动校准

- [ ] 6.1 完成布局/spacing/alignment/clip/overflow、长样例 tiles 覆盖和 window-layer popup ROI；通过缺 tile、错误裁图、自报过小 bounds、边缘外漏与错误刚性原点的正负控制，禁止图像缩放配准。
- [ ] 6.2 完成实际文本/font face/run/line/baseline、Icon identity 与局部墨迹/轮廓比较；用 Latin/CJK/bidi/ligature/选区确定性输入测试检出缺字、错字体、裁字和图标实际漏绘，不能只检查 metadata。
- [ ] 6.3 完成平坦颜色/边框/圆角/gradient/阴影/alpha 分层指标及显式 SDR/channel/color profile；用双背景透明校准、错通道/色域/硬断点/重复 alpha 负例验证，不能用 SSIM 或全图均值单独通过。
- [ ] 6.4 实现 pinned-font 与 system-font profile、font hash/face/fallback 检查；测试缺字/意外 fallback/系统字体错配失败，保留旧 page acceptance 的 system_fonts 合同与普通 Gallery 默认字体行为。
- [ ] 6.5 实现重复正控制、留出 corpus、最小负控制与 hard cap 的自动校准；通过 1/1.25/1.5/2 的数学输入、噪声过大不可分离、阈值自动放宽和过量 mask 负例，校准失败不能出 pass。
- [ ] 6.6 补齐真实 fault corpus：颜色、圆角、裁切、正式图标缺失、错字体/字号、clip/alpha，连同第 4 阶段故障明确所需检出层；通过单元/meta 协议测试，实际 GPU 红绿留在对应平台项验收。
- [ ] 6.7 写比较算法/度量单位/阈值来源/字体与色域边界文档，运行该阶段 PYTEST 和 CORE 定向测试，记录硬上限与未校准环境，不将输入数学测试冒称平台 GPU 覆盖。

## 7. 平台通用：完整行为、时钟和生命周期

- [ ] 7.1 扩展受控 accept/reject、focus/Tab/Shift+Tab/Escape、popup/childModal、wheel 消费与边界 oracle；用错误受控值、callback 次序、背景穿透、错 focus/disabled 负例验证实际 dispatch 回执与最终状态。
- [ ] 7.2 实现编辑/选区单位映射、composition 合成 seam、旧 session/迟到 commit、隐藏恢复和敏感草稿清理；通过 CORE 与 Web adapter 单独合同，明确 DOM、Core、SDL 及未测 OS IME/AT 证据层。
- [ ] 7.3 实现可控异步完成/取消、动态 key/拓扑/草稿及 owner-local 退役/idle oracle；通过乱序完成、删 key 迟到回调、shell Drawer 保留、共享字体缓存与额外 deadline 负例。
- [ ] 7.4 完成 JS clock 与 CSS animation timeline 控制/paint 观测、native existing clock 的统一 step 时间；用 t0/中间/终点/反向、timer/transitionend/隐藏取消自动 canary 验证，无法控制采样时非零且不使用 wall sleep 伪证。
- [ ] 7.5 建立逐组件 Provider 字段消费/显式覆盖矩阵与来源锁，加入 Progress 不继承 size/disabled、direction 与显式 LTR 的负控制及真实消费者正控制；自动断言不把所有组件统一生成继承预期，不改 Progress 数值实现。
- [ ] 7.6 扩展真实逻辑 fault corpus 到错误 focus、disabled 响应、未退役 deadline，保留 callback/value 注入；通过 PYTEST/CORE/自动 Web adapter 正负例并更新语义 oracle 文档，实际 GPU 平台结果单列。

## 8. 平台通用：按实际目录扩展映射与合同

各小项按实际 sample 拆为可独立提交的批次；每批必须补 source/adapter/feature 映射、独立 oracle、适用 variant 与自动 schema/coverage/adapter 合同测试。新增能力立即进入 pending 缺口，不能直到 full job 才发现。该阶段不以 adapter 单测代替后续平台 GPU 全覆盖。

- [ ] 8.1 完成 General 与四个 Token 内建 sample 的当前所有映射，覆盖文字/图标/局部 Theme 与组合上下文；自动核对每个实际身份/target 与官方来源及现有 native-only 边界。
- [ ] 8.2 完成 Layout 类所有当前 sample 的布局、窄宽、滚动/clip、受控与动态 topology 映射；通过该类独立 oracle 和漏 target/slot 负例。
- [ ] 8.3 完成 canonical Input 的五家族及编辑变体映射，保留一个目录项和独立 family 分母；通过文字、选区、输入/清除、Password/OTP 离页策略与多对多来源校验。
- [ ] 8.4 完成其余 Data Entry 当前 sample，包含 picker、Form 异步/动态列表、Select/TreeSelect、Upload 等明确网络/文件边界；通过每个 feature 适用性与单侧/paired oracle 测试，不把未映射当 unsupported。
- [ ] 8.5 完成 Navigation 当前 sample 的 focus/弹层/childModal/scroll/RTL/overflow 映射；通过执行动作与 callback/状态/生命周期正负例及该类 coverage 检查。
- [ ] 8.6 完成 Data Display 当前 sample，含 Table/Listy key/draft/分页及媒体/图片/动态内容；使用当时冻结 owner 提交和实际 writer 复算增量，覆盖 tests 不能删除旧身份或放宽为仅 count。
- [ ] 8.7 完成 Feedback 当前 sample，动态取样与 Progress 的非继承规则/正式已冻结数值合同分层，保留 owner 边界；通过来源锁、适用维度和 oracle 正负例，不导入未冻结数值研究产物。
- [ ] 8.8 完成 Other 当前 sample、List/Util 说明页和 shell native-only 合同，复用冻结 Other helper 的生命周期接口；通过完整有序 inventory join，保证当前所有 applicable obligations 都有 ready case、没有无依据豁免。

## 9. 平台通用：定位报告、自动复跑与 CI 选择

- [ ] 9.1 完善 visual/layout/typography/token/state/input/logic/lifecycle/environment/reference drift 分类、首次分歧、source/Token 对应与完整 build/renderer/font/source 指纹；通过错误归类和 dirty-source 未重建等 report/provenance 负例。
- [ ] 9.2 实现 `replay --failure --affected` 与 case 依赖闭包，保存旧失败及新 run；用组件/共享 Input/Theme/font/renderer、rename/delete/新增 case/未知路径测试精确或保守选择，缺失依赖不能静默漏测。
- [ ] 9.3 实现独立 reference migration/lock 与 profile/meta 检查，拒绝用 native 输出更新 baseline；通过更换 Web source/fixture/expected 的漂移负例及同参考重跑测试。
- [ ] 9.4 接入仓库 CI 的 targeted、pilot/meta 和显式 full shards、资源/超时预算与取消报告；通过 CI 配置/选择器测试证明未执行非绿、不强迫每次全 CTest/GUI、不创建用户定时任务，发布操作命令及 cold/warm 统计口径。

## 10. Linux：扩展后实际 GPU 自动验收

- [ ] 10.1 在实际 Linux/GCC Release、Vulkan/SPIR-V、私有 Wayland 上完成 pinned/system-font profile 校准及 expanded pilot variants；记录真实 DPR/色彩条件、Web renderer 与 native GPU、正控制重复和负控制检出，不把未运行 DPI 算通过。
- [ ] 10.2 运行当前全部适用 obligations 的 `run --suite all --coverage required`，严格核对选中/完成/通过身份集合、Input 家族、全部 sample/target 映射与显式边界；所有缺口/真实差异保持失败，修复后同参考及影响集自动重跑。
- [ ] 10.3 运行完整真实 visual/logic fault corpus 与协议/漏执行/宽 mask meta tests，全部要求红绿闭环；同时验证 actual SDL-mapped canary、composition seam、GPU fence/回读以及异常清理，无桌面 fallback。
- [ ] 10.4 定向回归旧 page runner/受影响 native 入口、cold-cache 全链与 warm targeted/replay，写简短 Linux 结果与实测预算；正常 callback 和 hidden readback 分列，不扩成 105/106 无关全矩阵或 OS IME/AT 声明。

## 11. Windows：实际 MSVC 与 D3D12 自动验收

- [ ] 11.1 在真实 Windows 使用正式 windows-msvc Debug/Release 构建受影响 Gallery/platform target，验证 ABI/toolchain/路径/退出清理与 SDL event mapping；不能用 MinGW/Wine/Linux 结果替代。
- [ ] 11.2 在 hidden D3D12/DXIL 和独立 headless browser 上完成 pinned/system-font profile 校准、pilot 及完整当前 applicable case 集；记录实际 Windows font/DPR/driver/color/readback，不复用 Linux 图像 baseline。
- [ ] 11.3 在上述 Windows 真实路径完成全部 visual/logic faults 的红绿与错版本/漏图/漏动作/crash/timeout/宽 mask 负例，确认不弹桌面错误框、不移动物理光标、子进程清理和非零失败传播。
- [ ] 11.4 验证 Windows cold-cache 准备、targeted/replay 与受影响旧 page 入口，独立写简短 Windows 结果及预算；不重复要求平台通用逻辑 CTest，也不把 105/106 的旧平台余项强塞进每次 109 回归。

## 12. 平台通用：最终一致性与边界文档

- [ ] 12.1 汇总最终 inventory 有序集合、全部 case/feature 覆盖账本和两平台独立结果，自动校验所有 ready/required case 的源码锁、oracle、profile/meta 证据及 replay 入口；未完成平台不得行政勾选。
- [ ] 12.2 完成开发文档、简短 evidence 与本 change artifacts 一致性校验，运行 doctor/strict/diff check、tracked-artifact 检查和相关文档命令自检；确认无 raw 图像/日志/运行 JSON 入库、无人工/Agent 读图 pass gate、无 107/108 任务向量修改。
