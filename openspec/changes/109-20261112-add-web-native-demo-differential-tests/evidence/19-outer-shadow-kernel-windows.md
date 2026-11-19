# Windows 外阴影有限 Gaussian 积分回归

- Windows MSVC HEADLESS Debug、Release 各通过 2 项定向 CTest。新增矩形可分离积分和圆形闭式解对照，覆盖实际 packed 参数与逻辑参考。Windows MSVC Release Gallery、校准程序及 5 项 shader 合同通过。
- 隐藏 D3D12/DXIL、实际 DPR 1.5 的同一训练板回读中，无填充圆角阴影相对于固定 Web 的可见总覆盖偏差由 28.7% 降至 17.0%，半透明填充由 32.7% 降至 20.7%。主体中心继续保持 RGB 255。
- 无填充控制的局部最大归一化 alpha 残差仍为 0.03529。外侧覆盖和主体边缘仍超出当前 5% 与 0.02 上限，继续判定失败，未发布校准 profile。该结果来自 Windows 单板诊断。
