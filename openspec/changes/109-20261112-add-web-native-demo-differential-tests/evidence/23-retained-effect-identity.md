# 实际提交效果的保留态身份

- Windows MSVC HEADLESS Debug 和 Release 各通过 native observation 定向 CTest。普通 Tag 回归从实际提交的描边恢复 store ID，并核对 generation 与 packed index。
- Linux GCC Release Gallery 在 WSL Ubuntu 24.04 私有 Mutter Wayland、llvmpipe Vulkan/SPIR-V、DPR 1 下完成 Select 的 10 个原生检查点，行为断言通过。正式绘制观测读取了 15 个外部镂空阴影，以及 5 个须从实际 effect store 查找的描边实例。
- 这次验收覆盖实际原生绘制身份与行为。三例配对校准和像素判定仍未通过。
