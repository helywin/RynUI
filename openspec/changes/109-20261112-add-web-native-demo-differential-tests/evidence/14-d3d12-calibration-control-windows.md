# D3D12 校准接入与首批控制复验

2026-11-19，Windows/MSVC `windows-msvc-release`，隐藏窗口 D3D12/DXIL 和锁定 Chromium 141.0.7390.37。旧 Web 准备目录因 Windows 系统库指纹变化被拒绝，使用 `bootstrap.py --mode BUNDLED --offline` 在新目录重新准备成功。

- D3D12 校准使用 SDL 从实际 DXGI adapter 取得的名称和用户态驱动版本，保留两个可选描述字段的原值。重跑通过 GPU 指纹检查，进入训练控制采集。驱动版本和可选字段一旦发生变化，都会改变 profile 的环境指纹。
- Chromium 同一文档中的前后查询复用 CDP 文档根，每次重新定位控件。原失败样本 `p0.glyph0` 的位置、字号、字形数和字体散列相同，差异来自 `DOM.getDocument` 重发的临时节点 ID。修复后，锁定浏览器对同一控制请求连续 **5/5** 次完成捕获及前后观测一致性检查。
- 原生控制在一张固定控制板上复用同字号、字重和替代策略的字体链，按实际加载字节和代际验证 SHA-256。Gallery 与校准目标 Release 构建通过，HEADLESS Debug/Release 的控制几何 CTest 各 **1/1**，Python 定向检查 **160/160**，含 runner **11/11** 和 meta **29/29**。clang-format 22.1.3、OpenSpec doctor/strict 与 diff 检查通过。

最终二进制重跑 `calibrate --suite pilot --dpr native --repetitions 5 --offline` 已完成训练控制的双侧 **5/5** 次采集。实际 DPR 为 1.5。透明填充的白底控制 `p1.shadow-no-fill` 中心回读为 native `[217,217,217,255]`、Web `[255,255,255,255]`，复跑得到相同结果。半透明填充的同位置分别为 `[236,236,236,255]` 与 `[255,255,255,255]`。两侧提交的材料证据和原始像素保存在 ignored `out/`，独立控制间的差异未计作三例 Gallery 的视觉比较结果。

首板逐像素复算发现扫描阴影时覆盖了控制列表，导致双背景 alpha 检查前抛出 `TypeError`。增加同板阴影和 alpha 的回归后修复，相关 Python 检查 **46/46**。用修复后的比较器重算本次运行第一对实际训练回读，命中 `shadow.independent-gaussian-profile` 和 `font.independent-outline` 等门槛。阴影可见贡献最大误差为 **0.1490**，超过固定上限 **0.02**。文字轮廓距离为 **4 physical px**，超过固定上限 **1 physical px**。这是实际首板诊断，校准通过需要完整正向、留出和故障控制全部达标。
