# 全部样例归属与 frame 迁移

2026-11-03，实际 Linux / `linux-gcc` Release / Ninja Multi-Config / BUNDLED。

- 48 个组件页的 63 组真实样例和 Token 的 4 组均有工厂；全部 73 个 canonical 路由与 Token 页通过隔离检查。未实现/已弃用条目生成实际说明文本，不挂交互演示；Upload 与 List 文本分别核验。
- 冻结全部 458 个历史 stable test ID 的唯一主 sample；4 项 replacement 在对应活动页解析并核对 owner。caption、Token 与交互目标有各自含义，不声称所有 ID 都是交互控件。
- `rynui_gallery_sample_page_tests`、`rynui_gallery_page_model_tests`、`rynui_token_gallery_frame_tests` 构建通过。样例页、model 与 14 组组件 frame 定向 CTest 共 16 项均通过；仅重跑失败项，没有再次运行已通过的整组。后补最后定向集合 `gallery_page_model|gallery_sample_page|gallery_page_time-picker_frame` 为 3/3 通过。
- frame 组包括 Select/Card/Tabs、Tag 等公共反馈、Table 联动、Collapse/Segmented、Empty/Result、Form、TimePicker、DatePicker、Breadcrumb/Steps、Descriptions/Skeleton、Popover/Message/Notification/Timeline、Avatar/Statistic/Rate、Tree/AutoComplete、Modal。保留输入、业务接受/拒绝、表单、popup、pointer/key/wheel、glyph/fill/hit、clip、focus、主题/resize 与 retained identity 断言。
- 多组件历史渲染合同使用明确路由列表的内部兼容夹具，每条路由调用已登记工厂；单活动页与退役由独立页面夹具验证。兼容夹具的旧目录外壳不是第二种交互产品模式。旧全页固定 307 live samples、编辑器/Select 的全局偏移以及阴影/目录卡/章节/滚动条的全局 surface 偏移已移除。
- 额外有限入口 `--frame-contract-only`、`--legacy-shell-only`：2/2 通过，保留记录上传、动画截止/稳定 idle 和旧外壳回归。
- 迁移暴露 TimePicker 旧断言只识别 Quad、并把正文 margin 当窗口边界。现在同时核验共同裁剪填充 RoundedEffect 的实际颜色、零 blur/spread、覆盖及列/弹层 clip；window layer 按真实窗口边界核验，正文仍保留独立 content clip。未改 Core 渲染行为。

clang-format 22.1.3：0 failures；OpenSpec doctor healthy，strict 105/105；diff 检查通过。以上是无窗口 CPU scene、模拟文本平台/Recording 回归；正式页面 shell、代码展开 UI、真实 GPU、系统 IME 和 Windows/MSVC 未由此证明。原始日志只在 `out/105-implementation/20261103/`。
