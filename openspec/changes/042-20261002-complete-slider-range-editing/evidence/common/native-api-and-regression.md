# 原生 API 与完整通用回归

2026-10-02，Windows/MSVC，windows-msvc-headless Debug/Release 完整 build 与 CTest。

- Debug 39/39，100.81 秒；Release 39/39，13.24 秒。新增 interaction_registry 进入 portable suite。
- SliderRef focus/blur/autoFocus：未绑定、复用、双重绑定拒绝、owner thread、销毁 generation、mount rollback、disabled/empty、相邻 sibling 隔离与 mount-only 聚焦通过。
- Hint optional placement 默认 horizontal Top/vertical Right；overflow=false 保留越界位置，true 调整；非法配置保留旧状态。
- Gallery 增加整段、editable MultiSlider、per-handle disabled 三个示例，live_samples=67。Slider 支持目录更新为原生 implemented，Web DOM/CSS/portal/兼容别名明确排除；Windows/GPU 与 Linux checkbox 独立。
- clang-format 22.1.3 检查 395 自有源码，0 failures；doctor healthy；full strict 42/42；git diff --check 通过。

本阶段不声称实际 Windows GPU 输入或 Linux 窗口通过；native Gallery build/受影响 CTest 与 GPU readback 在第 6 阶段保存。
