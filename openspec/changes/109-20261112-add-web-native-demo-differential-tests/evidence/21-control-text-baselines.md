# 独立文字控制的排版与栅格基线

- Windows MSVC Release 与 Linux GCC Release 均构建了校准程序。Windows 通过 11 项校准 Python 定向测试，所有 1242 个自有源码通过 clang-format 22 检查。
- Windows 隐藏 D3D12/DXIL 与锁定 Chromium 各观测 192 个单字控制。在相同 DPR 1.5 训练板上，两侧各自的原始 RGBA SHA 与增加观测之前一致。
- 独立 SFNT 核对首次定位出最大 0.760 logical px 的排版基线差，超过 0.5 上限。现有文字轮廓、颜色及外阴影也仍有不合格值。未发布 profile。
