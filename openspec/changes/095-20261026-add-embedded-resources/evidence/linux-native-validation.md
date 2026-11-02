# Linux 原生内嵌资源验收

2026-11-02，Arch Linux x86_64 / GNOME Wayland / NVIDIA RTX 5070 Ti Laptop / Vulkan，实际 shader format 为 SPIR-V，默认字体来源为 Fontconfig system。

- 正式 `linux-gcc` Debug/Release、`linux-clang` Debug configure/build 通过；Ninja Multi-Config / BUNDLED。
- Clang Debug 的 56 项受影响合同全部通过，包含 `embedded_shaders`、shader reflection、字体、资源与生命周期。
- 将新构建的 minimal、text、button、layout、Token Gallery 共五个程序分别复制至三组无 `shaders/`、`fonts/` 的独立目录，从另一 cwd 用 `--smoke` 实际启动：15/15 exit 0，Vulkan/SPIR-V 正常。
- Text 示例原先传入空 SceneDeviceMetrics，导致 smoke 失败；现在逐帧使用实际 drawable extent / display scale，三个正式配置重新构建和搬移 smoke 均通过。

最小命令：`cmake --build --preset <linux-gcc-debug|linux-gcc-release|linux-clang-debug>`；`ctest --preset linux-clang-debug -R 'embedded_shaders|generated_shaders|default_font_chain|platform_lifecycle'`；独立目录中的 `<example> --smoke`（环境 `SDL_VIDEODRIVER=wayland SDL_GPU_DRIVER=vulkan`）。

原始 logs、复制的程序及 SHA256 索引仅在忽略目录 `out/linux-validation/20261102/relocated/`；此项验证内嵌 shader / 系统字体，不宣称已制作完全独立的系统库分发包。不重复 Windows 验收，不覆盖系统 IME 或人工跨显示器检查。
