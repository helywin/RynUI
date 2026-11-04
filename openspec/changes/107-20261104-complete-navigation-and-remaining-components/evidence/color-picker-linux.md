# ColorPicker Linux 平台通用合同

实际环境：Linux / GCC 16.2.1；`linux-gcc-headless`，Ninja Multi-Config，Debug/Release，构建均为 `-j2`。配置为 HEADLESS + Recording；输入通过真实 Core 路由注入，字体为锁定验证字体。

共同定向组合在 Debug **12/12**、Release **12/12** 通过，覆盖两组件、Form、Theme、实际 Gallery 页面及目录/源码元数据。Gallery 定义库在两配置实际编译；`rynui.gallery_color_number` 各 **1/1**，80 个 source samples，458 个冻结 legacy IDs/owner 保留。

`python scripts/format-code.py --check` 使用 clang-format 22.1.3，899 个文件、0 失败；catalog generator/check/self-test、`openspec doctor --json`（healthy）、strict **107/107**、`git diff --check` 通过。五份完整 Theme golden 保留原有 58 个顶层字段及 Menu light/dark palettes、Dropdown arrow metrics 和 Anchor 字段，并加入 ColorPicker；未截取局部 shape。

边界：未执行真实 GPU/原生窗口、系统字体/系统 IME、桌面或 Windows/MSVC 验收；HEADLESS 结果不替代这些证据。63.2/64.2/65.1 保留给父线程。原始日志、上游源码副本与生成元数据仅在忽略的 `out/component107/`，不提交附件。

组件合同在两配置各 **1/1** 通过：RGB/HSB/Hex/alpha 与严格解析、精确 RGB 文本往返、真实 SV/hue/alpha logical scene、渐变色标及受控拒绝/键盘、清理捕获/编辑会话/preedit、回调销毁/异常、格式权威、可直接调用的 typed panel/text renderer、动态预设/clear/add/remove 实际按钮、受控重入取消与有效 CSS、disabledAlpha 不透明候选、模式渐变缓存、保留 panel/text/extra 与失效 render ticket、Theme 分域/Dark/Compact/独立算法。没有组件私有 GPU/SDL 路径或新 renderer ABI。

复核入口（Debug 构建 preset 对应 `linux-gcc-headless-debug`，Release 对应 `linux-gcc-headless-release`）：

```text
cmake --preset linux-gcc-headless
cmake --build --preset linux-gcc-headless-debug --target rynui_color_picker_component_tests rynui_gallery_pages_host_tests rynui_theme_algorithm_tests -j2
cmake --build --preset linux-gcc-headless-release --target rynui_color_picker_component_tests rynui_gallery_pages_host_tests rynui_theme_algorithm_tests -j2
ctest --preset linux-gcc-headless-debug -R 'rynui.(color_picker_component|gallery_color_number|theme_algorithm)$'
ctest --preset linux-gcc-headless-release -R 'rynui.(color_picker_component|gallery_color_number|theme_algorithm)$'
```

## 主分支后续原生组合

`linux-gcc` Release 私有后台 Wayland/Vulkan/SPIR-V/system fonts：ColorPicker → InputNumber → Button **3/3 cases、345 checks、86 submitted frames**，input-area/cursor 服务均实际调用。包含真实控件回调、拖动/编辑/连发/滚轮和持 preedit/capture 的离页清理，详见 [主分支组合验收](color-number-integration.md)。宿主事件注入为合成；没有桌面 fallback，不证明系统 IME 候选 UI、跨实际显示器、AT 或 Windows/MSVC。63.2/64.2/65.1 保持开放。
