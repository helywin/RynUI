# FloatButton Gallery 与 Linux 原生结果

2026-11-04，对应任务 1.2。canonical 页 `ant.component.float-button` 已注册真实 `gallery.sample.float-button.actions`：独立按钮、四向/常驻/悬停/受控 Group、共享显式滚动源的裁剪内容视口与 BackTop，以及真实窗口层可拖动入口。[公开说明](../../../../docs/float-button.md)。旧 Gallery 的 458 个迁移 ID 保持不变。

平台通用结果见 [核心合同](float-button-core.md)：Linux GCC HEADLESS Debug/Release 各 17/17；目录生成器 `python tools/update_ant_design_gallery_catalog.py --check --self-test` 通过。恢复冻结的 legacy inventory 后，只复验受影响的 `rynui.gallery_sample_page` 与 `rynui.ant_design_reference_catalog`，Debug/Release 各 **2/2 通过**。

原生构建和有界自动验收：

```text
cmake --preset linux-gcc
cmake --build --preset linux-gcc-debug --target rynui_token_gallery -j 2
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Debug/rynui_token_gallery --preset linux-gcc --configuration Debug --timeout 60 --log-dir out/106-float-native-final --page ant.component.float-button --page ant.component.button
```

实际 Linux / GNU / Debug / Ninja Multi-Config，私有后台 Mutter/Wayland，**Vulkan / SPIR-V / system fonts**。结果：**2 cases、31 checks、5 submitted frames，passed=true、return_code=0、desktop_fallback=false**。覆盖真实窗口锚定、拖动、捕获释放、键盘激活、文档滚动后位置保持、背景像素回读，以及离页销毁时取消未完成捕获。输入来自组件宿主的合成事件分发，GPU、窗口与像素回读在实际后台环境运行；没有操作用户桌面或系统鼠标键盘。

格式检查 clang-format 22.1.3：850 个自有源文件、0 failures；OpenSpec doctor 健康、strict 文档校验 106/106、`git diff --check` 通过。原始日志、JSON 与图片仅在 ignored `out/`，未提交运行附件。

边界：该原生流程定向覆盖窗口浮动按钮，不代表全部 Group/BackTop 状态的 GPU 像素矩阵。Windows/MSVC、系统 IME、实际桌面及跨显示器体验未实测，不勾选 change 的 Windows 或总体集成任务。人工体验不作为完成前置项。
