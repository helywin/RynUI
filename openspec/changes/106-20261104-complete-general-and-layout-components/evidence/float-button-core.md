# FloatButton 平台通用合同

2026-11-04，Linux / GCC 16.2.1，`linux-gcc-headless`，Ninja Multi-Config，Debug/Release。对应任务 1.1。

实现 typed FloatButton/Group/BackTop、reactive Theme/Props、保留 slots、窗口四角定位与拖动、点击/悬停/受控菜单、禁用和分支输入清理。BackTop 使用显式滚动源和回调、宿主时钟、reduced motion 与按实际圆/方形边界绘制的 logical progress；穿过显示阈值继续滚动到零，隐藏时单独取消指针手势。分组拖动后开合保持触发器位置，多行内容按实际高度布局。

核心测试 target：`rynui_float_button_tests`、`rynui_portable_theme_runtime`、`rynui_portable_theme_algorithm`、`rynui_portable_theme_scope_component`、`rynui_portable_theme_public_api`、`rynui_portable_window_overlay`、`rynui_portable_tooltip_component`。构建均使用 `cmake --build --preset linux-gcc-headless-{debug,release} --target <上述 targets> -j 2`。

实际连同 Gallery 两项运行的命令：

```text
ctest --preset linux-gcc-headless-debug -R '^rynui\.(float_button_|portable\.(theme_runtime|theme_algorithm|theme_scope_component|theme_public_api|window_overlay|tooltip_component)$|gallery_sample_page$|ant_design_reference_catalog$)' --output-on-failure
ctest --preset linux-gcc-headless-release -R '^rynui\.(float_button_|portable\.(theme_runtime|theme_algorithm|theme_scope_component|theme_public_api|window_overlay|tooltip_component)$|gallery_sample_page$|ant_design_reference_catalog$)' --output-on-failure
ctest --preset linux-gcc-headless-debug -R '^rynui\.portable\.backend_boundaries$' --output-on-failure
```

结果：Debug、Release 各 **17/17 通过**，其中 FloatButton **9/9**、受影响 Theme/Tooltip/窗口层 **6/6**，Gallery/目录 **2/2**；Core/renderer/平台依赖边界另 **1/1 通过**。原有 Theme golden 保留不变，原有字段逐字比较；新增 token 和身份变化独立断言。

`python scripts/format-code.py --check --clang-format /home/jiang/code/RynUI/out/linux-validation/format-tools/bin/clang-format`：clang-format 22.1.3，845 个自有源文件，0 failures。OpenSpec doctor 健康、strict 全部文档校验 106/106，`git diff --check` 通过。

本记录证明平台通用逻辑和 HEADLESS logical scene 合同；Windows/MSVC、原生 GPU/窗口、系统 IME 与实际桌面体验不据此宣称通过。原生 Gallery 结果单独记录；原始日志仅在 ignored `out/`。
