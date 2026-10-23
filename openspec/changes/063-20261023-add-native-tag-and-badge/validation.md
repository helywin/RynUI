# 验收记录

## 环境与范围

- 基点：`4e258e2afe832323c934e4ebfc3fb2144fc1608f`；独立分支 `t3/native-tag-badge`。
- 规划提交：`37a0df71ffafce4275ad0152ade7ce0ef210bb8e`；实现提交：`56764b7b4a2735d0d97a484dcc84df097899f434`。两次提交后均 fetch/push 并核对远端；实现提交安全 rebase 至本分支上游，远端 SHA 与本地一致。
- Windows x64，MSVC 19.51.36256，`windows-msvc-headless`，Ninja Multi-Config，Debug/Release；编译命令使用 `--parallel 4`。
- 仅使用本工作树的 `out/build/windows-msvc-headless`。本地 helper 由获准的 root helper 复制，并调整并发为 4；未提交 helper、构建产物或本机路径配置。

## 通过的检查

- 三项专用 CTest：`rynui.tag_badge_public_api`、`rynui.tag_badge_theme`、`rynui.tag_badge_component`。
- 六项相关回归：`rynui.portable.theme_algorithm`、`theme_runtime`、`focus_lifecycle`、`window_overlay`、`card_component`、`modal_boundary`。
- Debug 与 Release 均为上述 **9/9**。未重复运行全仓库 CTest。
- `python scripts/format-code.py --check --clang-format <clang-format 22.1.3>`：496 个自有源码，0 失败。
- `openspec doctor --json` healthy；`openspec validate --all --strict --no-interactive` 62/62；`git diff --check` 通过。
- 用既有 `rynui_portable_theme_algorithm --write-goldens` 生成五份主题快照；逐份解析并与 HEAD 比较，除新增 tag/badge 字段与 identity 外，所有既有 Token 数据完全相同。catalog、sources.lock 和 support overlay 未改动。

## 行为证据

专用测试覆盖受控关闭/选择（拒绝写回时保持原状态）、icon/content 不重跑、隐藏/恢复身份、指针、Enter/Space、禁用焦点及命中清理、焦点环与键盘按压材质、零/窄尺寸、logical translation、无活动时不请求帧、关闭回调同步销毁、资源清理；Badge 覆盖 count/overflow/showZero/dot/status/text/offset/size、独立及包裹布局、indicator 覆盖 child 时的输入穿透、主题颜色不测量和状态颜色局部更新。

主题测试覆盖锁定默认色/尺寸、Default/Dark/Compact、继承与重置、component algorithm、Token 订阅分组、非法配置原子拒绝、snapshot identity 与序列化。

## 交付文件

- 公开 API：`include/ryn/tag.hpp`、`badge.hpp`、`status_types.hpp`、`tag_badge_tokens.hpp`，及 umbrella/theme 接线。
- 实现：`src/component/tag_badge_component.{hpp,cpp}`；局部窗口服务注册、主题解析/订阅与 CMake 接线。
- 测试：`tests/tag_badge_{public_api,theme,component}_tests.cpp`；五份 theme golden。
- 使用文档：`docs/tag-and-badge.md`；规划、spec、tasks 与本记录位于 063。

## 验收边界

未执行真实 GPU 人工视觉、Linux/Wayland 或系统 IME。Gallery、综合目录、README、completion 和 support overlay 由父任务处理。首版具体不支持项见组件文档；processing 为静态状态色，未声称动画完成。CPU scene v3/GPU ABI v2 与 renderer 源码未改动。
