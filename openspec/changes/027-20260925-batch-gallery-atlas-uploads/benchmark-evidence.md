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
