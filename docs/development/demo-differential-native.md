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
- Core 选区是 **UTF-8 byte offsets**，text cluster/line byte indices 显式标注；DOM 是 UTF-16。`text_units.py` 仅转换完整 scalar 边界，grapheme 合法性仍由 Core 合同决定。不能把 scalar 数直接当作 byte/DOM 偏移。
- checkpoint 经现有 `save_frame_bmp` → GPU readback/fence，绑定本次 nonce/scene revision/实际 metrics/文件 hash；`completed_readbacks` 只数本 adapter 成功的 checkpoint 导出。正常 Wayland callback、逻辑 submitted frames、checkpoint readbacks 分列。现有 renderer `gpu_submissions` 指标在 hidden composition/export 路径可为 0，不代表未执行这些独立 fence readback，也不作为正常 surface callback 数。

每次执行写严格 `hello`、每动作 `receipt`、每 checkpoint 新图、最终 `complete`。主 runner 拒绝缺动作/目标/图/结束、旧 nonce/scene/source、错误 DPR 和捕获 hash。输出目录不可复用。GPU、源码副本和完整协议/环境材料只在 out。

## 相关自动验证

正式 HEADLESS Debug/Release 定向 targets：`rynui_demo_differential_protocol_tests`、`rynui_demo_differential_native_input_tests`、`rynui_gallery_page_acceptance_tests`；CTest 对应 `rynui.demo_differential_native_{protocol,input}` 与 `rynui.gallery_page_acceptance_{options,runner}`。真实 bridge 测 pointer/Tab/Enter、controlled accept/reject、真实 callback/commit 故障恢复、组件各自 eligibility、单 sync wheel、retirement/stale query，以及实际 Input base/selected/placeholder views。

平台通用 CPU 通过不代表 GPU/平台输入验收。实际 Linux 观测、旧失败和单位定义迁移见 change109 evidence04；当前还没有完整 Web/native paired、校准 profile、fault 红绿或 Windows 验收。
