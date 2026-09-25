# Gallery buffer transfer 基线

Windows 11 10.0.26200、Intel Core Ultra 9 285HX，正式 `windows-msvc` / Ninja Multi-Config / MSVC Release `--clean-first` 构建 `rynui_token_gallery`。实际 SDL D3D12 / DXIL，1280×900 logical，display scale 1.25。五个独立进程分别执行 `--scroll-acceptance`，每次自动滚动 240 步、240 个采样帧且退出码 0。原始字段见 `gallery-transfer-before.csv`，完整输出在被忽略的 `out/build/windows-msvc/transfer-baseline-*.txt`。

| 指标 | 五进程中位数或稳定计数 |
| --- | ---: |
| 整帧平均 CPU | 4,153 µs |
| 帧 CPU p95 | 5,078 µs |
| 资源同步 CPU | 1,334 µs |
| Quad / Glyph / Effect / batch finish | 约 158 / 1,000 / 24 / 175 µs |
| buffer 上传区域 | 2,848 |
| buffer transfer 创建 / 映射 | 2,848 / 2,848 |
| texture transfer 创建 / 映射 | 0 / 0 |
| 全生命周期 GPU 上传提交（含首次挂载） | 1,320 |

计数在自动滚动开始时取快照，在第 240 个提交帧截断，因此 transfer 指标仅覆盖滚动窗口。CPU 阶段计时包含计时器和调度开销；提交阶段不等于 GPU 执行时间。当前每个 buffer 区域恰好对应一次 transfer 创建和映射，约 11.9 次/采样帧。下一阶段用有界同帧 transfer 合并验证固定成本是否下降，并保留相同场景与 telemetry 复测。
