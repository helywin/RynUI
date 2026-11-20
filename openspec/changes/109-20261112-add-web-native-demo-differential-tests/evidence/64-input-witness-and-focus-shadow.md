# 受控 Input 字形、光标及焦点阴影

使用正式 Windows MSVC Release 规则重建并运行 Input paint snapshot 和 RoundedEffect packed reference。两项通过，覆盖方形物理像素孔径、镂空、四档模拟比例与 Input retained 场景。比较器定向 CPU 测试 91 项通过，包含双侧漏墨、来源偏移、光标断行和阴影尾部几何的负控制。

固定 Chromium 与隐藏 D3D12 的实际 1.5× DPR 捕获共三个检查点。独立设备光栅见证确认两枚重叠 CJK 字形的字面、位置与像素孔径一致，两侧实际 RGB8 仍逐像素校验量化范围。焦点阴影改用带镂空的设备像素覆盖后，受控 Input 的 initial、accepted、rejected 在固定 policy 上限的定向比较中均为零差异。这次使用定向回读和比较，三例联合 profile、meta 与其他平台仍分别验证。
