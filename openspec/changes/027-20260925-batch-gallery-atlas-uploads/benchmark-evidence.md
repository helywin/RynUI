# Gallery 首次帧 atlas 上传基线

Windows 11 10.0.26200、Intel Core Ultra 9 285HX，正式 `windows-msvc` / Ninja Multi-Config / MSVC Release `--clean-first` 构建 `rynui_token_gallery`。五个独立进程执行 `--scroll-acceptance`；实际 SDL D3D12 / DXIL，1280×900 logical，display scale 1.25。每次完成 240 步自动滚动且退出码 0。首次成功提交帧独立计时，不混入滚动 240 帧平均。原始字段在 `gallery-atlas-before.csv`，完整输出在被忽略的 `out/build/windows-msvc/atlas-before-*.txt`。

| 指标 | 五进程中位数或稳定计数 |
| --- | ---: |
| 首次成功提交帧 CPU | 473,334 µs |
| 首帧资源同步 CPU | 288,363 µs |
| 首帧 Glyph 同步 CPU | 285,971 µs |
| 首帧 GPU 上传提交 | 1,080 |
| 首帧 atlas 纹理上传 / transfer 创建 | 1,077 / 1,077 |
| 首帧 buffer transfer 创建 | 3 |
| 后续 240 帧滚动平均 CPU | 约 4.1 ms |

当前 `upload_glyph_texture` 为每个纹理区域创建 transfer、map/unmap、copy pass 和命令提交。首帧资源同步占整帧约 61%；1,077 次单独提交与该热点一致，但目前只有阶段计时，不能把全部 286 ms 都归因于提交固定成本。下一阶段只合并同帧命令/pass，保持纹理 transfer 创建数量不变，以测得的首帧差异判断实际收益。GPU 执行时间未测。

## 共享 copy pass 后的真实窗口对照

代码 `f3d71a9`，同机同配置正式 Windows MSVC Release 完整 `--clean-first` 构建。五个独立 D3D12 / DXIL 进程均完成 240 步自动滚动且退出码 0；原始字段见 `gallery-atlas-after.csv`，完整输出在被忽略的 `out/build/windows-msvc/atlas-after-*.txt`。

| 指标 | 改造前中位数 | 改造后中位数 |
| --- | ---: | ---: |
| 首次成功提交帧 CPU | 473,334 µs | 377,440 µs |
| 首帧资源同步 CPU | 288,363 µs | 123,109 µs |
| 首帧 Glyph 同步 CPU | 285,971 µs | 121,180 µs |
| 首帧 batch finish CPU | 0 µs | 1,093 µs |
| 首帧 GPU 上传提交 | 1,080 | 1 |
| 首帧 atlas 纹理区域 / transfer 创建 | 1,077 / 1,077 | 1,077 / 1,077 |
| 首帧 buffer transfer 创建 | 3 | 3 |
| 首次挂载加滚动期 GPU 上传提交 | 1,320 | 241 |
| 滚动 240 帧平均 CPU | 4,123 µs | 4,101 µs |
| Quad / Glyph / Effect draw | 2,190 / 2,178 / 553 | 2,190 / 2,178 / 553 |

首帧 CPU 中位数减少约 20%，资源同步减少约 57%；五次首帧总时长的前后取值区间未重叠。这个收益来自共享提交/pass 与命令编码路径的整体改变，不能把全部差值归因于单个 SDL 调用；纹理 transfer 的 1,077 次创建和映射仍保留。滚动终态、区域、draw 数与退出码保持一致。未测 GPU 执行时间，也未做逐帧人工像素检查。

Windows MSVC Debug 定向 `rynui.buffer_upload_batch_layout`、`rynui.glyph_atlas`、`rynui.glyph_gpu_resources` 3/3 通过；Debug 真实 D3D12 `--scroll-acceptance` 完成 240 步且首帧上传提交为 1。Release 的 `--input-acceptance`、`--selection-acceptance`、`--password-acceptance`、`--input-clear-acceptance` 均一次通过。
