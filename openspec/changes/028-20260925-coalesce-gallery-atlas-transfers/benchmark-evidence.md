# Gallery atlas transfer 分块复测

旧基线直接复用 027 的五进程真实 D3D12 `../027-20260925-batch-gallery-atlas-uploads/gallery-atlas-after.csv`；该版本已把纹理与 buffer 编码到同一个 copy pass，但首帧仍为每个纹理区域创建/map/unmap 单独 transfer。新版本代码 `8f6a1c9` 在同一 Windows 11 10.0.26200 / Intel Core Ultra 9 285HX 机器，以正式 `windows-msvc` / Ninja Multi-Config / MSVC Release 完整 `--clean-first` 构建。五个独立进程运行 `--scroll-acceptance`，实际 SDL D3D12 / DXIL，1280×900 logical，display scale 1.25；均完成 240 步自动滚动且退出码 0。新原始字段见 `gallery-texture-chunk-after.csv`，完整输出在被忽略的 `out/build/windows-msvc/texture-chunk-after-*.txt`。

| 指标 | 027 旧基线中位数 | 028 新结果中位数 |
| --- | ---: | ---: |
| 首次成功提交帧 CPU | 377,440 µs | 189,322 µs |
| 首帧资源同步 CPU | 123,109 µs | 5,294 µs |
| 首帧 Glyph 同步 CPU | 121,180 µs | 3,336 µs |
| 首帧 batch finish CPU | 1,093 µs | 895 µs |
| 首帧 GPU 上传提交 | 1 | 1 |
| 首帧纹理区域 / transfer 创建 | 1,077 / 1,077 | 1,077 / 5 |
| 首帧 buffer transfer 创建 | 3 | 3 |
| 后续 240 帧 buffer 上传区域 / transfer 创建 | 2,848 / 251 | 2,848 / 251 |
| 后续 240 帧平均 CPU | 4,101 µs | 4,156 µs |

首帧 CPU 中位数减少约 50%，资源同步减少约 96%；五次首帧 CPU 取值区间前后未重叠。1,077 个纹理区域、一次 GPU 上传提交及固定滚动工作量保持一致。后续滚动平均 CPU 差异约 55 µs，处于该场景的帧节奏和运行波动范围内，不宣称滚动进一步加速。更短的首帧使验收结束前多运行了动画帧，首次挂载起累计的 draw/上传提交数因此不同，不能以这些生命周期累计值推断绘制结果改变；每次固定 240 步和真实窗口验收均成功。GPU 执行时间和逐帧人工像素等价未测。

Windows MSVC Debug `--clean-first` 定向构建后，`rynui.texture_upload_batch_layout`、`rynui.buffer_upload_batch_layout`、`rynui.glyph_atlas`、`rynui.glyph_gpu_resources` 4/4 通过；真实 D3D12 Debug `--scroll-acceptance` 完成 240 步，首帧纹理 transfer 为 5、上传提交为 1。首次增量 Debug 构建时 Gallery 源文件没有随 `SdlSceneRenderer` 类布局变化重编，导致 ABI 不一致和退出前访问冲突；构建日志与崩溃转储支持这一诊断。移除临时诊断后，干净重建的相同源代码窗口验收通过。Release `--input-acceptance`、`--selection-acceptance`、`--password-acceptance`、`--input-clear-acceptance` 均一次通过。
