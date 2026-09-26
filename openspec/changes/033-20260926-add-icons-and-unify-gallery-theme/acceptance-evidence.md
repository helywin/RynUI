# 验收记录

## 规划

用户已授权自主创建 change 并实现；本次追加滚动条悬浮/按下颜色反馈。033 strict validate 通过，全仓 27/33，通过项含 032/033；既有 013、015、016、017、018、021 失败。当前 CLI 不支持 `doctor --json`，且 `new/status/instructions` 拒绝数字开头的项目规范名称；按既有 spec-driven 结构手工建档。`git diff --check` 通过。此时仅规划完成，代码尚未实现。

## 图标资源

Ant Design 6.6.5 依赖 icons 6.3.4，其 SVG 包依赖范围从 4.6.0 起。已锁定 `@ant-design/icons-svg` 4.6.0 发布归档和 commit，九个官方单色 SVG 及 MIT 许可证随源码保存。FontTools 4.60.1 仅用于离线维护，生成器固定轮廓容器时间戳；`--check` 逐字节复现通过。Windows MSVC Debug `rynui.icon_asset_contract` 与 `rynui.dependency_lock` 2/2 通过。资源完成不代表公开 Icon 已实现。

## Icon 基础组件

公开 typed `IconProps` 支持 reactive name、tone、visible 和外部 LayoutStyle。内部沿用 Text 的 retained glyph 场景、Atlas、Theme/semantic foreground 及局部材质更新，但使用独立的内嵌图标轮廓字体，不进入普通文字 fallback chain。字体缓存以逻辑字号和实际 display scale 分离。

Windows MSVC Debug `rynui.icon_asset_contract`、`rynui.text_scene_service`、`rynui.text_component`、`rynui.text_component_frame` 4/4 通过。新测试从真实内嵌资源生成一枚非空眼睛位图，验证缓存重用、暗色文字颜色、不重跑父组件、切换眼睛状态时保持 identity/兄弟字形，以及 150% DPI 创建独立缓存。此时消费控件尚未切换图标。
