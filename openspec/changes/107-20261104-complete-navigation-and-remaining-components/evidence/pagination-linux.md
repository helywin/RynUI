# Pagination Linux 平台通用验收

- 基线：`f20a77415d8f37c185f0de74dfe5f570cbbed06a`；上游 Ant Design 6.6.5 / `4a39f54842eade4e565ab336ef6097cd7e723cdd`。源码/API/demo 下载仅在忽略的 `out/upstream/`。
- 实际环境：Linux x64，GCC 16.2.1，`linux-gcc-headless`，Ninja Multi-Config；构建始终 `-j2`。
- 实现和覆盖：页大小选择/独立受控 authority、快速跳页校验、simple/readOnly、总数范围、响应式对齐及保留 typed item 内容；非法输入/total 收缩、零数据/整数溢出及回调销毁。
- 构建：`cmake --build --preset linux-gcc-headless-<debug|release> --target rynui_pagination_component_tests rynui_token_gallery_frame_tests -j2`。
- CTest：`ctest --preset linux-gcc-headless-<debug|release> -R '^rynui\.pagination_component$' --output-on-failure`；Debug **1/1**、Release **1/1**。
- 实际 Gallery 定义及 Recording frame executable 已构建；`out/build/linux-gcc-headless/tests/<Debug|Release>/rynui_token_gallery_frame_tests --data-only` 在 Debug/Release 各 **1/1** 通过。保留 frozen legacy IDs。
- `format-code.py --check`（clang-format 22.1.3）通过；目录生成/自测、OpenSpec doctor、strict **107/107**、`git diff --check` 通过。
- 边界：这是平台通用逻辑/Core 输入分发的合成事件/Recording scene 证据；未执行原生窗口/GPU/系统 IME、辅助技术或实际 Windows MSVC。本子分支不勾选 63.1、64.*、65.1，不 push、不建 PR、不 archive。
