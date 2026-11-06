# QRCode Linux 来源验收

这是隔离来源 `cee72c95f57deb307effb7a12d9a3978f12acb4b` 的模型阶段记录，当前 main 验证见 [model 集成](qrcode-model-integration.md)。

基线 `20e67d510535d5accd23c1ef70989fa1ef7a3aa0`，当前阶段仅为标准编码模型与 CPU 导出，38.1/38.2 尚未关闭。

- Linux GCC，`linux-gcc-headless`，Ninja Multi-Config，Debug/Release，构建 `rynui_qrcode_model_tests -j2`。
- `ctest --preset linux-gcc-headless-{debug,release} -R '^rynui.qrcode_(model|independent_decode)$' --output-on-failure`：每配置 2/2 通过；模型 89432 个断言，独立 ZBar 7/7 PNG/SVG 解码（ASCII、UTF-8、含 NUL 的 binary、透明 logo）；SVG 由 librsvg 渲染，PNG 的 chunk CRC/zlib 由独立 Python 标准库验证。
- 独立 Segno 1.6.6 固定 v1/Q/mask0、v7/M/mask4、v40/H/mask7 全矩阵指纹，覆盖 numeric/alphanumeric 与 version/format/mask；byte 路径以 ZBar 实际解码验证。Segno 对恰好字节对齐的数据额外插入一个 zero pad byte，故该情形不要求两个有效实现有相同矩阵，未修改编码器或参考库。
- CPU 导出为应用持有的 RGBA8、确定性 PNG 与 SVG；无文件读写、网络、renderer 或 SDL。PNG 使用整数 module pitch、额外空间居中填充，保留 quiet zone；SVG 保留 logical 模块。图标不保证任意尺寸/透明背景/纠错/对比度组合均可扫描。
- 原始日志、上游源、独立工具及导出仅在忽略的 `out/qrcode-tour` 与本工作树 `out/build`。未以此阶段证明原生窗口/GPU/组件生命周期；Windows/MSVC/D3D12、实体 IME/AT/跨显示器未验证。
