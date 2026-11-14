# Web / native demo 自动差分

109 的唯一入口是 `python -B tools/run_demo_differential.py`。现有 inventory 入口保留，增加 `list`、有限 `run` 和失败集 `replay`。runner 调用已有锁定官方 Web prepare/execute API 与原生 scenario CLI；实际 pilot、有效平台校准与完整 factory 适配尚未验收，不能把 source/单元控制称作视觉或行为通过。工具不以人工或 Agent 读图裁决。

## 有序 inventory 与分母

```text
cmake --preset linux-gcc-headless
cmake --build --preset linux-gcc-headless-release --target rynui_demo_differential_tests -j2
python -B tools/run_demo_differential.py inventory --binary out/build/linux-gcc-headless/tests/Release/rynui_demo_differential_tests --output out/demo-differential/inventory-check-1
```

`--output` 必须是当前 checkout `out/` 下尚不存在的目录。Gallery 本身也接受独占参数 `--page-inventory=<out/file.json>`，在创建窗口、字体、GPU 或帧泵之前返回；不需要 display、compositor 或 Web 安装。HEADLESS 测试程序链接同一个生产 registry/exporter，检查编译时数据与当前源码 writer 的完整有序向量，而非数量。

当前声明基线 `a45a83e85524e3662fd098daf34bbb9cd3af2942`：

| 向量 | 当前数量 | 口径 |
| --- | ---: | --- |
| catalog | 73 | canonical 组件，Input 仅一项 |
| routes | 80 | 77 个组件/家族合法路由（含 List、Util 说明页），另 3 个内建路由 |
| native component/family routes | 75 | 上述组件路由去除 List、Util；与旧 visual discovery 的口径一致 |
| samples / sources | 114 / 114 | 110 个组件样例，另 4 个 Token 样例 |
| legacy targets | 490 | 每项保留实际 sample owner 与 replacement alias |
| coverage skeleton | 757 | catalog、route、sample 可见区、target 各自独立 obligation |

这些是冻结基线的观察值，不是未来上限。`identity-baseline.json` 是已声明的身份/顺序/owner/source 合同，特意不存运行结果、截图或源码副本。新增身份立即形成 coverage 缺口；删除、同 count 换身份、重排、owner/source 迁移需要明确修订该合同，不能调整数量或通过子集检查消除。baseline 保留全部现有 target，含原始迁移目标与后增身份。support status 的独立文案变化不构成身份迁移。

实际 `inventory.json` 包含编译后的 source excerpt，留在 ignored out；工具重新运行 `generate_gallery_source.generate()`，逐项验证其函数、公共 header、真实代码及顺序。source 行号/代码改变而未重建也会拒绝。catalog 内容摘要同时核对 source manifest、overlay 和两种既有 schema。alias 必须无环、保持同一 sample owner，或由该 owner 的真实 factory 消费新目标身份。

`coverage.json` 是声明性映射，当前全部 `pending/planned`。`implementation=ready`、映射类型和本次 execution 是不同轴；planned、缺 case、not-run、fail/error 均不计 pass。List/Util 仍须说明页合同，不能据此豁免其他已有 native 能力。后续 feature matrix / case 映射将在 109 的对应阶段扩充，现有 skeleton 不声称所有 feature 已实现。

## 协议维护与验证

六种 version-one schema 位于 `tests/fixtures/demo-differential/`，schema 源定义在 `tools/demo_differential/schema_definitions.py`。只存 schema、声明性输入和映射。修改 schema 源后执行：

```text
python -B tools/demo_differential/schema_definitions.py
python -B tools/demo_differential/schema_definitions.py --check
python -B -m unittest discover -s tests -p 'demo_differential_*_tests.py'
ctest --preset linux-gcc-headless-debug --no-tests=error -R '^rynui\.demo_differential_'
ctest --preset linux-gcc-headless-release --no-tests=error -R '^rynui\.demo_differential_'
```

当前协议基础仅用 Python 标准库，不安装或隐式发现第三方依赖。内置 validator 仅解释这些 schema 实际使用的 JSON Schema 2020-12 vocabulary，遇到未知 vocabulary 失败；不承诺任意外部 schema 的完整实现。JSON 限制为 16 MiB、48 层，重复字段、非有限数字、未知协议字段、危险相对路径、重复身份/step、时钟回退、无 action 的 checkpoint、缺 oracle 和 native-derived expected 均拒绝。公开 Props/数据等领域 map 允许声明的 JSON 数据，不接受可执行 grammar。

所有原始 inventory、报告、日志、源码副本、图片与二进制只写 ignored out。上述自动测试验证协议/覆盖与 CPU 编译 registry，不证明 Web、GPU readback、真实呈现、Windows 或系统 IME/AT。

## 有限 pilot runner

```text
python -B tools/run_demo_differential.py list --suite pilot
python -B tools/run_demo_differential.py run --suite pilot --reference locked --preset linux-gcc-release --binary out/build/linux-gcc/examples/Release/rynui_token_gallery --profile-file out/demo-differential/calibration/profile.json --output out/demo-differential/pilot-fresh
python -B tools/run_demo_differential.py replay --failure out/demo-differential/pilot-fresh/report.json --binary <fixed-gallery-binary> --output out/demo-differential/pilot-replay-fresh
```

`run` 的 prepare 自动调用既有 `bootstrap.py --mode BUNDLED` 和中央字体资源准备；已有独立安装可显式传 `--prepared <out/prepared>`、`--font-files <out/fonts/font-files.json>`，仍逐项验证。`--comparison-site` 只接受独立 bootstrap 校验过的 Pillow 安装；不发现全局库、不改锁。Windows x64 Web artifact 与隔离 CPython 3.14/Pillow 准备路线已明确锁定，入口见 [比较器解释器准备](demo-differential-comparison.md#windows-x64-解释器准备)。必须用该 ABI 的实际解释器启动整个 runner，并传其已验证 site；Python 3.10 仅可引导准备，不能加载 cp314 wheel。CPU 准备通过不代表字体、浏览器页面、校准或实际 Windows pilot 已通过。

状态顺序为 select/inventory → prepare → preflight → execute → validate → compare → report → cleanup。`--preset` 是 build preset，由 CMakePresets 解析 configure preset/configuration 并核 hello；HEADLESS/Recording 不能用于 paired。native preflight 使用同一真实 sample 的首个 checkpoint scenario 读取实际 DPR，再仅将 DPR 输入传给 Web；不从 native measured bounds 或 pixels 生成 Web 样式/expected。显式 `--dpr` 必须匹配实际 native metrics，默认 `native`。Linux 始终使用既有私有 Wayland/DBus wrapper，Windows 使用 hidden native 路径及无 console/错误弹窗的进程创建。单 native、单 Web 串行，不另造帧泵、组件或 renderer。

两侧原始 v1 记录口径保留：native receipt 在 checkpoint 前且 capture nonce 独立；Web checkpoint 在 receipt 前，使用 run nonce、step、递增 paint sequence 证明新 readback。runner 分别严格验证顺序、完整集合、hash、scene/fence 与实际 metrics，不重写原始文件。oracle view 只映射 Web 实测 focus 字段名及输入层名称；callback 的 target/key/value、重复、顺序全部保留，继续使用既有独立 `oracle.py`。

退出码：0 为全部选中要求通过；1 为完整证据有差异；2 为配置/schema/coverage/reference drift；3 为准备/实际环境/校准错误；4 为 crash/timeout/缺动作、图、complete、新帧或中断。多个 case 失败取 `4 > 3 > 2 > 1`，每例保留原因。畸形路径字段属于 schema 2；已声明但缺失的当前图、旧 nonce/frame 属于 incomplete 4。旧 output 拒绝复用，超时清理本次拥有的进程树；缺私有 compositor 不回退桌面。

启动 adapter 前预写失败 `report.json`；结束原子发布 strict report v1、JUnit 和 escaped 本地 `index.html`。原始 streams/images/logs、source manifest、resolved definition、比较指标、geometry、规范化 trace 和可生成的 diff PNG 全在该新目录，case 结果指向相对路径。报告保留 selected/expected/completed/passed 原始身份集合、首次分歧 step、层分类、阈值来源及 replay argv。`replay` 保留旧目录并核原 case/fixture/oracle/profile hash，允许换 binary/preset；定义或 profile 改动拒绝伪称同条件修复。`--affected`、全量选择/CI 尚未实现；有限真实 fault 的外层入口见下节，平台红绿仍须实跑。

三个 source-bound 定义位于 `tools/demo_differential/pilots.py`，绑定真实 sample/function/header、完整有序 legacy target、固定 official demo/组件 hash，以及 Small/Middle、default 局部 Theme、中文标签、260 宽度、Input 前后缀/32 上限、desktop/service/unavailable 数据和受控 accept/reject。`pilot-coverage.json` 是现有 coverage v1 的 pilot 子账本，保持 `planned`；完整样例/实际输入接口与校准尚缺时绝不能计 pass。`profiles/pilot-pinned-font.uncalibrated.json` 仅是拒绝通过的 schema 模板，零摘要/指标不是校准证据。

输入消费证据使用不可变 `resources.actual_initial_inputs` 和每次动作后的 `resources.actual_inputs`，从 mounted host、实际 editor、retained slot/ThemeScope/布局及官方 committed fiber/connected host 读取。Select 保留完整 factory 的 numeric/variants、multiple/tags/virtual/async、局部 Theme、prefix 和真实 popup 子控件；Web 初始关闭的合法 popup 可保持 deferred，首次挂载与声明的截止检查点仍须证明实际实例、seed、归属及真实输入消费。静态 options/说明结构继续逐次核对，异步数据由独立动作 oracle 校验。反馈通过真实 Text 实例及 `feedbacks` map 观察，逐步预期只取独立声明的 equals literal。它们不构成成对视觉通过或通用动态 status 覆盖。

pilot 的 1280×900 表示 logical/CSS viewport；Windows 窗口坐标先按实际 display scale/pixel density 转换，再核对真实 logical/physical metrics。样例内宽 672 来自 `GalleryPagesHost` 的 `gallery_page_layout.hpp` 及样例 padding 声明，不读 native measured bounds 回填 Web。分组标题的内部不可选 marker 与 Web public disabled-group 分开，子选项 value 类型、顺序、label、disabled 始终独立核对；native 无法表达的 public disabled-group=true 明确拒绝。

本阶段纯 Python 验证：

```text
python -B -m unittest discover -s tests -p 'demo_differential_runner*_tests.py'
python -B -m unittest discover -s tests -p 'demo_differential_pilots_tests.py'
python -B -m unittest discover -s tests -p 'demo_differential_native_resources_tests.py'
python -B -m unittest discover -s tests -p 'demo_differential_font_requirements_tests.py'
```

runner 测试可注入有限故障 adapter 和 analytic comparator；它们输出的 report 显式带 `mechanism_controls=true`。这些验证编排、退出优先级、原始身份与拒绝路径，不证明真实 browser/native/GPU pilot 通过；没有自动更新任何 tasks/support checkbox。

## Pilot 故障外层

```text
python -B tools/run_demo_differential.py meta-test --suite pilot --reference locked --preset linux-gcc-release --binary out/build/linux-gcc/examples/Release/rynui_token_gallery --profile-file out/demo-differential/calibration/profile.json --output out/demo-differential/meta-fresh
python -B tools/run_demo_differential.py run --suite pilot --fault button-padding-plus-one --reference locked --preset linux-gcc-release --binary out/build/linux-gcc/examples/Release/rynui_token_gallery --profile-file out/demo-differential/calibration/profile.json --output out/demo-differential/padding-fresh
```

`--fault` 仅进入 acceptance scenario，普通产品路径仍为 `none`。有限集合为 `button-padding-plus-one`（真实 Small/default Button token）、`input-reject-commit` 和 `duplicate-callback`（真实受控 Input 回调）；不修改已保存图片或报告。普通 faulted run 永远不 pass；只有 marker 而无独立差分时返回协议错误 2，不能冒充成功检出的差分 1。

meta 先要求完整三例正控制绿，再逐 fault 要求指定 case 为差分 1、其他两例绿、首次失败分类正确，随后去 fault 的三例复跑绿，共七个 fresh ordinary run。padding 可由 layout/token/visual 检出，错误受控提交为 state/logic，重复回调为 logic。环境错误、漏动作/图/complete、错层失败、仅有 marker 或恢复仍红，都使 meta 自身非零；正控制未绿不执行“预期失败即通过”的捷径。

所有 meta 子运行及显式 fault run 都禁止 reference cache publish；cache hit 仍验证完整性、实际 browser environment 与图像 hash，cache miss 仍执行 Web 动作。四个 binding 固定参考 cache keys、完整有序 case definitions、profile hash 和 native source hash，逐轮检查输入及先前产物不被修改。正常 schema-v1 report/JUnit 保留真实 verdict，不注入 `expected_failure`。开始子运行后的外层报告在 `<output>/runs/meta-report.json`，prepare 失败报告在 `<output>/meta-report.json`。CPU 机制结果见 [evidence09](../../openspec/changes/109-20261112-add-web-native-demo-differential-tests/evidence/09-runner-meta-controls.md)；它不代替实际平台红绿。

## Native 3.4–3.6 冻结接口对接

兼容层消费 [native 观测接口](demo-differential-native.md) 的 observation v1 resource maps，不改顶层 schema。checkpoint 的 `resources.readback.run_nonce` 绑定当前 run，独立 `nonce` 与该保存 capture 的 nonce/step 相等；另核严格递增 readback/fence id、scene revision、稳定 renderer owner/device epoch、五个实际 device metrics 字段、actual source channel 及 source/binary hash。窗口坐标和提交/读回统计保留在原始 metrics，不能与 GPU device metrics 混淆；相同 scene revision 仍须有新 readback/fence。capture 的 RGBA 是 normalized readback bytes，`framebuffer_format/source_channel_order` 保留实际 RGBA/BGRA target，不把 BMP 存储通道混作 renderer 通道。

`resources.lifecycle` 按实际 serialization 的平面字段验证：sample `owner_generation`、带 component/page generation 的 `owners`、带整数 `active`/nullable deadline 的 `animation_scopes`、实际存在的 `auxiliary_deadlines`、sample live counts 与 focus/capture/session/retired；独立保留 `window_deadline_us`、`shell_owner_present` 和 `shared_font_cache_policy`。window 非空 deadline、合法 shell 与 window-owned retained font cache 不算 sample 泄漏；不猜造 `cache_count`。`idle_observation=not-sampled` 不证明 passive idle，readback 的 compositor/presented null 也不证明显示器呈现。

当前 pinned 准备链为七文件：Latin/CJK 各有真实 upright 400/500/600，以及覆盖 U+1F642 的单色 emoji400。Gallery source/support paragraph 需要 500，shell/Select 需要 600；缺任一需求时 runner 在 native probe 前返回 environment **3**，不会重标 regular、合成字重或用系统字体补齐。`font_requirements.py` 核对显式文件 bytes/hash、实际 SFNT face/weight/slant、有序链和重复身份；资源来源与准备命令见 [差分验收专用字体](demo-differential-fonts.md)。普通 Gallery 系统字体默认不受影响，源码与 CPU 检查不表示 paired 已通过。
