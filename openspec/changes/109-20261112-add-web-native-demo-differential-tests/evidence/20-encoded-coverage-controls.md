# 独立校准板 RGB8 覆盖核验

- Windows 纯 Python 定向通过 51 项，覆盖量化端点、外框位移及缺失、阴影投影和原有分层、runner 合同。
- 在已有 Windows D3D12 与固定 Chromium 的原始训练板上复算，低对比平面和半透明 AA 误报消失。192 个文字外框的轮廓从 168 个无法闭合恢复为全部可测，最大轮廓距离为 0，局部覆盖偏差约 0.36%。检测仍保留实际覆盖积分和颜色码差。
- Linux GCC Release 构建了 Gallery 与校准程序。WSL Ubuntu 24.04 的私有 Mutter Wayland 下，Vulkan/SPIR-V 通过 llvmpipe Mesa 25.2.8 完成隐藏训练板回读，DPR 1、1280×900、1,152,000 个不透明像素，独立 BMP 读取与原始帧核对一致。该次为单端软件 Vulkan 工具链验证。
- 校准 profile 仍未发布。部分形状的局部覆盖偏差仍约 13.2%，文字和阴影也仍需验证。
