# Linux 圆角与反馈验收

2026-11-02，实际 Arch Linux / GNOME Wayland / NVIDIA RTX 5070 Ti Laptop / Vulkan SPIR-V，GCC 正式 Debug/Release。

- `rynui_token_gallery --rounded-acceptance`、`--switch-acceptance`、`--radio-acceptance`：render scale 为 system/1/1.25/1.5/2，两配置均通过（共 30 次）。内部注入验证输入、motion/reduced、idle 和 dispose；Rounded 实际 resize 另由读回尺寸确认。
- `python tools/verify_native_gpu_feedback.py out/linux-validation/20261102/native --work-dir out/linux-validation/20261102/gpu-pixel-checks`：10 组通过；440 个圆角区域、560 个手柄中间帧、80 个 Radio 填充案例；最大通道误差 1/255，声明容差 2/255。包含全部 16 种 corner mask、浅/深、LTR/RTL、双尺寸与释放连续性；Rounded resized readback 均为 1420×1000。
- 修复验收夹具在 Wayland 初次 configure 前固定 projection 的问题；窗口最大化时先 restore 后 resize。未改 HLSL、renderer ABI 或组件视觉合同。

完整环境及边界见 [Linux 验收摘要](../../../../docs/validation/linux-native-20261102.md)。结果来自当时原生桌面上的程序窗口，用户随后要求后台运行后已停止该方式；独立 headless compositor 的动画等待超时，不作为这些原生通过结果的替代。render override 不代替跨真实输出或系统 IME。

原始 BMP/RGB/CSV/JSON/log 仅在忽略目录 `out/linux-validation/20261102/`。不重复 Windows 或已通过的平台通用逻辑，不提交原始产物。
