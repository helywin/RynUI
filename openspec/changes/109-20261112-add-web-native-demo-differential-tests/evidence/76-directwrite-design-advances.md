# DirectWrite 字宽与字形相位

- pinned DirectWrite 后端在整形前按设计字宽截断到 16.16，保留 OpenType 定位。字形位置先加半个相位再取下界。此修复替代证据 63 对四分之一相位中点的处理。
- Windows MSVC HEADLESS Release 的 font runtime、glyph scene 和 text engine 3/3。同期 Windows D3D12 与 Linux 私有 Wayland/Vulkan Release 定向 CTest 各 9/9。Linux 保留现有 Fontations 字宽路径。
- Windows 1.5× 中，Input 接受编辑后的末尾 `d` 使用相位 0，原两处过量墨迹回到参考的白色。Windows 1.5× 和 Linux 1× 的三个 Input 检查点在既有硬上限下均未检出差异。
- 另以独立彩色和中性字形诊断复跑 384 个训练控制及 312 个留出控制，两平台均未超硬上限。Windows 两板文字覆盖、轮廓与投影差为 0。此项使用单次真实采集，正式重复校准与 paired verdict 仍单独验证。
