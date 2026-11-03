# 其余页面工厂与普通草稿子阶段

2026-11-03，实际 Linux / `linux-gcc` Release / Ninja Multi-Config / BUNDLED。

- 登记 48 个 canonical 组件页的 63 个真实样例组，以及 Design Token 的 4 组；拆分 Tag/Badge/Drawer/Popconfirm/Alert/Spin/Progress、Collapse/Segmented、Empty/Result 混合构造函数。Table 保留编辑、分页、Menu/Dropdown 和数值编辑联合流程；Notification、ConfigProvider 提供指向已有流程的关联路由。
- 构建 `rynui_gallery_sample_page_tests`、`rynui_gallery_page_controller_tests`、`rynui_token_gallery_frame_tests`：通过。定向 CTest `rynui.gallery_sample_page`、`rynui.gallery_page_controller`：2/2 通过。旧 frame `--common-only`、`--panels-only`、`--states-only`：3/3 通过。
- 新页面夹具遍历全部 73 个 canonical 路由和 Token 页，检查无关工厂零调用、旧 surface 退役、说明条目不挂假演示、关联目标有效；保留原有 Input/OTP/TreeSelect/布局/导航断言，并验证空 Select 往返、Modal 普通文本恢复且 open 清空、Table 联合流程与业务记录恢复。
- 草稿字段按 page/family 显式登记，仅编码普通业务值。非受控编辑通过 default 值与组合回调恢复，未改公开 controlled/uncontrolled 规则。样例容器不再显示默认 planned 徽标。
- 这是 3.5 的工厂子阶段；3.5/3.6 的全部旧 458 ID 归属及历史 frame 迁移尚未收口，checkbox 保持未完成。正式原生 shell、代码展开、响应式几何仍未替换。

格式检查使用 clang-format 22.1.3，0 failures；OpenSpec doctor healthy，strict 105/105，diff 检查通过。以上为无窗口 CPU scene/模拟文本平台回归，不代表 SDL、GPU、系统 IME 或 Windows/MSVC。原始输出只在 `out/105-implementation/20261103/`。
