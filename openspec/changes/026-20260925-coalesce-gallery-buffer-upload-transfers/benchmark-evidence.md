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

## 合并后的真实窗口对照

代码 `49e38db`，同机同配置正式 Windows MSVC Release 完整 `--clean-first` 构建。五个独立 D3D12 / DXIL 进程均完成 240 步自动滚动且退出码 0；原始字段见 `gallery-transfer-after.csv`，完整输出在被忽略的 `out/build/windows-msvc/transfer-after-*.txt`。

| 指标 | 改造前中位数 | 改造后中位数 |
| --- | ---: | ---: |
| 整帧平均 CPU | 4,153 µs | 4,121 µs |
| 帧 CPU p95 | 5,078 µs | 4,364 µs |
| 资源同步 CPU | 1,334 µs | 269 µs |
| Glyph 同步 CPU | 1,000 µs | 12 µs |
| batch finish CPU | 148 µs | 80 µs |
| 提交阶段墙钟时间 | 2,653 µs | 3,691 µs |
| buffer 上传区域 | 2,848 | 2,848 |
| buffer transfer 创建 / 映射 | 2,848 / 2,848 | 251 / 251 |
| texture transfer 创建 / 映射 | 0 / 0 | 0 / 0 |
| GPU 上传提交（含首次挂载） | 1,320 | 1,320 |
| Quad / Glyph / Effect draw | 2,190 / 2,178 / 553 | 2,190 / 2,178 / 553 |
| 自动滚动 240 步墙钟时间 | 999 ms | 991 ms |

transfer 创建与映射减少约 91%，资源同步阶段约减少 80%；平均整帧时间只减少约 0.8%，不能宣称整体吞吐量有同幅提升。提交阶段包含 swapchain 等待，快路径释放的时间大部分转成等待；p95 减少约 14%。上传区域、字节数、提交数、draw 数、组件及滚动终态保持一致。未用 GPU profiler 测 GPU 执行时间，也未做逐帧人工像素检查。

Windows MSVC Debug 定向 `rynui.buffer_upload_batch_layout`、`rynui.quad_primitive`、`rynui.glyph_scene`、`rynui.rounded_effect_gpu_resources` 4/4 通过；Debug 真实 D3D12 `--scroll-acceptance` 完成 240 步。Release 的 `--selection-acceptance`、`--password-acceptance`、`--input-clear-acceptance` 一次通过；首次 `--input-acceptance` 因无法聚焦受控 Input 以代码 7 退出，紧接两次重试均通过。该一次焦点失败尚未定位成因，不计作已稳定解决。
