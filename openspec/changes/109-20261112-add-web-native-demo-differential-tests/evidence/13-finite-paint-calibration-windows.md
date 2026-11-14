# 有限绘制层观测与独立校准：Windows

2026-11-14，整合当前 R1/R2、R3/R4 冻结源码及 deferred-owner 修正。Windows/MSVC，平台通用检查使用 `windows-msvc-headless` Debug/Release；原生目标使用 `windows-msvc` Release。

- 显式非浏览器 Python 检查 **75/75**，覆盖校准证据闭包、正式图标来源、颜色与 source-over 可见贡献、分层区域、shadow profile/tail、错误像素、缺字和实际 device metrics。只运行本阶段十个指定测试文件，没有执行会隐式启动浏览器的全目录 discover。
- HEADLESS CTest Debug/Release 各 **4/4**：`demo_differential_control_geometry`、`input_paint_snapshot`、`demo_differential_native_input`、`demo_differential_native_observation`。真实 Input retained fragment 所有权、销毁后旧 generation、晚挂 popup 缺席及观测查询路径均保留。
- Release 的 `rynui_token_gallery` 与 `rynui_demo_differential_calibration` 构建成功。clang-format 22.1.3 检查 1237 个 tracked 源文件及四个本阶段新增 C++ 文件通过；Web 模块语法、profile schema 生成一致性、OpenSpec doctor/strict 和 diff 检查通过。
- 最终二进制在 hidden D3D12/DXIL 上执行 Button/Input/Select 三例，分别完成 **4/3/10 个检查点**；当前 source/binary/readback/resources 检查、输入消费时间线和独立语义 oracle 均通过。这是原生捕获和语义证据，不是 paired 视觉通过。

实现保留实际 command/instance membership、owner/generation、glyph atlas/font 来源、窗口五个稳定 device metrics 和独立 run/capture/fence 绑定。正式图标的 intrinsic layer opacity 与最终 paint opacity 分开验证。有限控制包含 312 controls、26 negatives、16 个固定上限指标；这些数量描述控制定义，不表示真实校准通过。

实际执行独立 `calibrate --suite pilot --preset windows-msvc-release --dpr native --repetitions 5 --offline`，使用最终 Gallery/control binary、锁定 Chromium 141.0.7390.37、CPython 3.14.0/Pillow 12.0.0 和七字体链。校准在环境绑定阶段返回 **3**：D3D12 提供了 GPU 名称和驱动版本，`driver_name`、`driver_info` 为空，首个拒绝原因是 `actual native GPU binding unavailable: driver_name`。没有发布 profile，未进入控制样本的重复采集/判定，后续 paired 与 meta 未执行；未填造驱动字段或放宽绑定要求。

原始运行、完整观测、图像及身份只保存于 ignored `out/`。本阶段不修改 tasks/support，也不启用产品 paint、Conic 或 nonRound 能力。Linux GPU、系统字体 profile、显示器 present、系统 IME/AT、完整动画和全目录组件覆盖没有本轮验收结论。
