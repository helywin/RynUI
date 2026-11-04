# Steps Linux 平台通用验收

- 基线：`f20a77415d8f37c185f0de74dfe5f570cbbed06a`；上游 Ant Design 6.6.5 / `4a39f54842eade4e565ab336ef6097cd7e723cdd`。源码/API/demo 下载仅在忽略的 `out/upstream/`。
- 实际环境：Linux x64，GCC 16.2.1，`linux-gcc-headless`，Ninja Multi-Config；构建始终 `-j2`。
- 实现和覆盖：动态 key 增删/重排及异常回滚、保留富编辑器、Default/Dot/Inline/Navigation/Panel、Inline 保留说明浮层/Escape、percent/maxCount、响应式/RTL、有限主题动画/失活清理。
- 构建：`cmake --build --preset linux-gcc-headless-<debug|release> --target rynui_steps_component_tests rynui_steps_theme_tests rynui_token_gallery_frame_tests -j2`。
- CTest：`ctest --preset linux-gcc-headless-<debug|release> -R '^rynui\.steps_(component|theme)$' --output-on-failure`；Debug **2/2**、Release **2/2**。
- 实际 Gallery 定义及 Recording frame executable 已构建；`out/build/linux-gcc-headless/tests/<Debug|Release>/rynui_token_gallery_frame_tests --navigation-only` 在 Debug/Release 各 **1/1** 通过。保留 frozen legacy IDs。
- `format-code.py --check`（clang-format 22.1.3）通过；目录生成/自测、OpenSpec doctor、strict **107/107**、`git diff --check` 通过。
- 边界：这是平台通用逻辑/Core 输入分发的合成事件/Recording scene 证据；未执行原生窗口/GPU/系统 IME、辅助技术或实际 Windows MSVC。本子分支不勾选 63.1、64.*、65.1，不 push、不建 PR、不 archive。

既有 children_steps/focus_steps 与 hidden/disabled/inactive/hidden_blur/disabled_blur 生命周期回归，在 Debug/Release 各 **7/7** 通过；含失焦中销毁父宿主。
