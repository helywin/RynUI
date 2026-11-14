# Gallery native scenario 与观测

109 的 native adapter 复用现有 Gallery factory、target registry、`pages.dispatch`、`acceptance_time`、OnDemandFrameLoop 和 renderer GPU readback。它输出执行证据；独立 oracle、Web 比较与最终 verdict 属于主 runner。旧 page runner 的 `passed` 不能替代差分判定。

## 有限入口

在正式 preset 构建 Gallery 后，由 runner 在 ignored out 写入 envelope：

```text
{schema_version:1, nonce, definition:<case v1>, fault, font_files:[{identity,path,sha256,face_index,weight}]}
```

既有 `--page-acceptance --page-visual --page=<route> --sample=<sample>` 加 `--page-scenario=<out/request.json> --page-observations=<out/observations.jsonl>`。仅一个显式 route/sample，与旧 i18n tour 互斥；两路径必须在当前 checkout out，输入有界、输出新建。typed parser 拒绝未知 action/parameter、重复/倒序 step、无 checkpoint、越界时间/指针比例、错误 source identity/font bytes。完整 case JSON Schema 先由 Python 校验，C++ 不执行 JSON oracle 或任意脚本。

pointer/key/text/composition/wheel 进入实际 `pages.dispatch`，外部策略动作仅切换已声明受控父应用的 accept/reject。初始化 window focus 是显式 Core seam，不使用 target `request_focus` 满足输入断言。当前未执行的 async/平台输入族不会据此计 pass。

## 来源与观察

- build 生成有序 source closure：实际 include/src/Gallery/shader/CMake/资源锁和生成器内容，不只记录 Git HEAD。runner 核完整向量/摘要、实际 binary SHA、configure preset/configuration；改源未重建失败。
- sample factory 的 begin/end 观测仅记录现有 retained roots，不增加布局容器。sample-context 包含完整 authored 内容，Gallery title/导航/源码壳单列；不按每个控件紧裁/配准。
- Button 使用现有 snapshot，Input 使用 editor/display/text layers，Select 使用 model/applied-state 查询；新增只读查询不重读业务 Prop getter、不改公开 Props。`resources.actual_initial_inputs` 与声明 fixture 分列，未具备的输入映射不会自动成为已验证。
- 文本读取 **const TextSceneService**，保留 source/view 共享 shaping。记录行、glyph cluster、font source bytes/face、实际 ink/clip；icon-font 单列，不把私有码点当普通文字屏蔽。
- `replacement_count` 的单位明确为 `invalid-UTF8-repairs`，不能证明字体覆盖。Gallery 验收查询实际选中 face 对原始 scalar 的 nominal glyph，另列 `font_coverage_available` 与 `missing_font_scalars`；没有查询或缺字不能作为 paired 字体通过。该查询只用于有限观测，不改变 Core 的 fallback/加载/渲染行为。
- Core 选区是 **UTF-8 byte offsets**，text cluster/line byte indices 显式标注；DOM 是 UTF-16。`text_units.py` 仅转换完整 scalar 边界，grapheme 合法性仍由 Core 合同决定。不能把 scalar 数直接当作 byte/DOM 偏移。
- checkpoint 经现有 `save_frame_bmp` → `read_frame_rgba`，返回由实际 submit/wait/map 产生的 `SceneReadbackEvidence`。保存文件的 bool 不充当 fence 证据。owner-local readback/fence 序号、scene revision、renderer owner/device epoch、实际 metrics 和源通道绑定到本次 nonce/checkpoint。相同 scene 允许保持 revision，但必须得到新的完成 receipt。`completed_readbacks` 只数本 adapter 成功的 checkpoint 导出；`gpu_submissions` 原样保留，0 不代表未执行 readback，也不代替正常 surface callback 数。

每次执行写严格 `hello`、每动作 `receipt`、每 checkpoint 新图、最终 `complete`。主 runner 拒绝缺动作/目标/图/结束、旧 nonce/scene/source、错误 DPR 和捕获 hash。输出目录不可复用。GPU、源码副本和完整协议/环境材料只在 out。

## 相关自动验证

正式 HEADLESS Debug/Release 定向 targets：`rynui_demo_differential_protocol_tests`、`rynui_demo_differential_native_input_tests`、`rynui_demo_differential_native_observation_tests`、`rynui_gallery_page_acceptance_tests`；CTest 对应 `rynui.demo_differential_native_{protocol,input,observation}` 与 `rynui.gallery_page_acceptance_{options,runner}`。真实 bridge 测 pointer/Tab/Enter、controlled accept/reject、真实 callback/commit 故障恢复、组件各自 eligibility、单 sync wheel、retirement/stale query，以及实际 Input base/selected/placeholder views。新 observation target 使用真实 retained Gallery 和明确的 CPU/mock capture port；不使用 SDL、不宣称 GPU fence 或图像验收。

平台通用 CPU 通过不代表 GPU/平台输入验收。实际 Linux 观测、旧失败和单位定义迁移见 change109 evidence04；本轮 Windows/MSVC 的 HEADLESS Debug/Release 与字体合同结果见 [evidence08](../../openspec/changes/109-20261112-add-web-native-demo-differential-tests/evidence/08-native-observation-contract.md)。完整 Web/native paired、校准 profile、fault 红绿与平台矩阵继续开放。

## Observation v1 的补充字段与未知数据

顶层、target 和 capture 仍符合既有 observation v1 schema，新增事实只进入其可扩展 map：

- `resources.readback` 在 checkpoint 包含 `run_nonce/nonce/checkpoint/readback_id/fence_id/scene_revision/renderer_owner/device_epoch/metrics/source_channel_order/source_hash/binary_sha256`，其他记录为 null。`run_nonce` 与 run 相等；独立 `nonce` 与当前保存 capture 的 nonce 相等，不能用 run nonce 冒充图片身份。五个 device metrics 字段逐项与窗口观察核对，coordinate extent/提交与读回统计是独立诊断字段。`capture.channel_order=RGBA` 表示 readback 已归一化的像素；`framebuffer_format` 和 `source_channel_order` 保留实际 RGBA/BGRA target。BMP 的文件存储通道由解码器处理。alpha 为未反预乘的 framebuffer bytes，`native-encoded-SDR` 不声称浏览器 sRGB 校准已完成。
- `resources.compositor_callback/presented` 为 null，`presentation_evidence=not-observed-by-checkpoint-readback`。readback 不观察实际显示器呈现，不能把默认 false 当作已经测得“未呈现”。
- static target 的 `key` 为实际 semantic identity，`state.key_source=static-semantic-target`；Select 的 `option_keys` 是当前 model 的有序 value。这里不宣称通用动态 List/Table key 观测。sample 的 `state.controls` 保留逐 target 状态，不将最后一个后代的 disabled/value 覆盖成整个 sample 的状态。
- 非编辑组件的 selection、没有相应语义的 disabled/loading/open 为 null；实际 snapshot 查询到的 false 才写 false。文本和 icon 数组来自当前 retained scene 的枚举，icon 各层带实际文本/字体/ink/clip。未知或过期 font source 抛错，不以空数组、零 repair count 或 fallback 标记判绿。
- `resources.lifecycle` 记录 factory 创建的 sample roots/后代和 animation scopes 的精确代际、逐 owner 存续、focus/capture/editor session、Button loading/Input caret/Select motion deadline；`window_deadline_us` 独立保留窗口事实。shell owner 和 window font/icon cache 是合法保留资源，不能要求全窗口归零。`deadline_coverage=sample-animation-scopes-and-pilot-auxiliary` 明确当前 pilot 查询范围，`idle_observation=not-sampled` 明确没有被动 idle 采样；它不代表其他组件的 timer 或后续生命周期 oracle 已实现。

`lifecycle` 是一个平面对象，字段含义如下；缺失字段不能用默认零或 false 补成已观测数据。

| 字段 | 实际含义 |
| --- | --- |
| `owner_generation` | 当前 sample 的 page generation；不是每个 component 的 generation。 |
| `owners[]` | `identity/present/branch_active/generation/page_generation`；component 的旧代际仍留在列表中，退役后 `present=false`，不可查询的 `branch_active=null`。 |
| `animation_scopes[]` | `identity/present/active/deadline_us/generation/page_generation`；`active` 是 scope 内活动动画的整数数量，旧 scope 的数量和 deadline 为 null。 |
| `auxiliary_deadlines[]` | 仅列实际存在的 `kind/owner/time_us`，kind 为 `button-loading/input-caret/select-motion`；空数组表示这些已查询的 pilot owner 当前没有 deadline。 |
| `live_components/live_scopes/live_interactions` | 已记住的 sample 所有权集合中仍存续的数量，不包含 Gallery shell 和共享字体缓存。 |
| `focus_owned/capture_owned/session_owned` | 当前焦点、鼠标 capture、文本输入 session 是否由该 sample 所有。 |
| `retired` | generation 为零时是 null；否则仅当上述三个存续数量归零且无 sample focus/capture/session 时为 true。它不单独证明全窗口静止。 |
| `window_deadline_us/shell_owner_present` | 整个窗口的 deadline 和 shell 存续事实，与 sample 的资源退役分开。 |
| `shared_font_cache_policy` | 固定声明 `window-owned-retained-not-page-leak`；当前没有 `cache_count` 字段。 |

旧 scope/input/component 代际的查询抛出 stale，旧 editor session 事件经同一 `pages.dispatch` 被生产 admission 拒绝。Gallery dispatch 对文本/文件输入返回实际派发结果，避免把进入路由但被 session 拒绝的事件标为 consumed。

## 显式字体与来源校验

`pinned` 要求 `fixture.fonts` 与 `font_files` 的有序 identity/hash/face/weight 一致，文件在本次 checkout out 中且 bytes hash 正确。测试入口设置 `allow_system_fallback=false`；普通 Gallery 仍使用默认系统字体链。pinned loader 不隐式加入系统字体、monospace 或 bundled replacement，resize 保持显式文件；缺少实际请求的 weight/slant 文件直接失败。例如 Gallery shell/Select 需要 600 字重时，只有 regular 文件的请求会报错，不能暗中解析系统粗体或把 regular 标成另一字重。

实际 font runs 根据持有 bytes/face 查询标为 `pinned-file/system-file/bundled-icon/bundled-fallback/unknown`，同时导出 source path（嵌入图标无文件路径）。pinned 普通文字必须匹配声明文件，system 普通文字必须来自已解析的系统文件；图标仅接受当前 TextSceneService 的真实 bundled icon cache 身份。system profile 的 `font_files` 可为空，host 不注入它们，paired runner 仍须检查声明和实际字体是否对齐。

原生启动 scenario 前逐文件检查嵌入 source closure；脏源码未重建或文件 hash 不符即失败。每个 checkpoint 再核实际 binary hash 与 hello 一致。runner 仍负责当前完整 source 向量/新增路径、binary/preset/configuration、case/oracle 与协议完整性检查；这些检查不能互相替代。

新增 C++ 正负例覆盖旧 frame/fence/scene、错 DPR/channel、错误 font-source/face/hash、source/binary 不一致、dirty source 未重建、owner-local 退役与合法 shell deadline。实际 MSVC/HEADLESS、default-font target、旧 finite runner 和 native GPU 结果分别记录；CPU/mock 通过不增加真实 paired 覆盖。
