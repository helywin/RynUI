# Image 透明目标读回主线补测：Linux

- 唯一来源 `f757e13c97b2342fc82d3670456e8556b7590664`，在已交付 C `d63acdcc2d447d1aa059ee0250a3b24168beb512` 后精确导入两文件 test-only 增量；来源结果仍标注来源，不代替本次验证。
- 实际 Linux/GNU，CMakePresets / Ninja Multi-Config：`cmake --build --preset linux-gcc-debug --target rynui_image_rounding_gpu_tests -j2`；`ctest --preset linux-gcc-debug -V -R '^rynui\.image_rounding_vulkan_private$'` **1/1、2,581,848 条检查**，0 失败。
- 私有 headless Mutter/Wayland 上实际 Vulkan 读回；四种缩放、透明 FBO 中 RGB/alpha 128/64/0/恢复逐像素对完整图参考。保留原测试全部断言，无容差放宽，无实现、资源、ABI、shader/backend 变化。
- clang-format 22.1.3 **1027 文件、0 失败**；doctor healthy、strict **107/107**、diff check 通过。change107 仍 **68/135**，目录/Theme 无改动。
- 独立交付 SHA 由本文件首次加入的 Git 历史标识；原始日志与读回仅 ignored out。未验证 Windows/MSVC/D3D12、物理 IME/AT 或实际跨显示器。
