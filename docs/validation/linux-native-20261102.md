# Linux 原生验收（2026-11-02）

实际环境：Arch Linux x86_64、GNOME 原生 Wayland、SDL Vulkan/SPIR-V、Fontconfig 系统字体；GCC 16.2.1、Clang 23.1.3、CMake 4.4.4、Ninja 1.13.2。正式 preset 使用 Ninja Multi-Config 和锁定 BUNDLED 依赖。

## 构建与平台回归

- `cmake --fresh --preset linux-gcc`、`cmake --fresh --preset linux-clang`：通过。
- `cmake --build --preset linux-gcc-debug`、`linux-gcc-release`、`linux-clang-debug`：通过。
- GCC Debug 受影响回归：`ctest --preset linux-gcc-debug -R '(hit_test|default_font_chain|notice_theme|timeline_theme|descriptions_theme|shadercross_override|animation_architecture|layout_demo|space_public)' --output-on-failure`，13/13 通过。
- Clang Debug 字体、平台输入、生命周期、shader、资源、依赖、hit-test、受影响 Theme/布局合同：56/56 通过。
- 修复 Linux 无法编译的共享字体角色枚举、fixture 相对 include；Fontconfig coverage 请求及字重单位；三个 Theme 的字体上限使用真实栅格器范围，避免依赖 Linux/Windows 不同的 `long` 宽度。
- 配置夹具补齐 SheenBidi，Space 负编译与工作流合同对齐当前 API/AGENTS；布局测试等待完整有限时长的 Button wave。

本节只记录构建及自动测试，不代表系统 IME、人工视觉或跨实际输出缩放验收。原始 log、GPU readback、CSV/JSON 等仅保留于忽略目录 `out/linux-validation/20261102/`。
