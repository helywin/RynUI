# Windows 外阴影主体镂空回归

标准均匀圆角 surface 的模糊外阴影在原始主体轮廓处镂空。阴影偏移和扩展只作用于投影轮廓。该规则对应 [CSS 外阴影的形状与镂空](https://www.w3.org/TR/css-backgrounds-3/#shadow-shape)。

- Windows MSVC HEADLESS Debug 和 Release 分别通过 7 项定向 CTest。原生 Release 构建 Gallery 和校准程序通过，RoundedEffect、Effect Paint 与 composition 的 shader 合同通过 5 项。
- 隐藏 D3D12/DXIL、实际 DPR 1.5 的训练板回读确认了镂空。透明填充的主体中心由 RGB 217 恢复为 255，半透明填充由 236 恢复为 255。两个中心均与固定 Chromium 141.0.7390.37 的原始回读一致。实际已提交阴影 Effect 及填充顺序通过控制板来源校验。
- 无填充控制相对于 Web 的完整剖面最大 alpha 残差由 0.14902 降至 0.03529，仍超出 0.02 上限。外侧模糊和边缘残差继续保留失败，校准 profile 未产生。本记录不提供其他 DPI 或 Linux 的实测结论。
