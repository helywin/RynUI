# Gallery 滚动同步基线

Windows 11 10.0.26200、Intel Core Ultra 9 285HX、MSVC 19.51.36248.0；正式 `windows-msvc` / Ninja Multi-Config / Release `--clean-first` 构建。实际 SDL D3D12 / DXIL，1280×900 logical，display scale 1.25。五个独立进程分别运行 `rynui_token_gallery.exe --scroll-acceptance`，每次 240 步、`exit_code=0`。原始字段见 `gallery-profile-before.csv`；完整进程输出保存在被忽略的 `out/build/windows-msvc/gallery-profile-before-*.txt`。

| 指标 | 五进程中位数或稳定计数 |
| --- | ---: |
| 整帧平均 CPU | 17,339 µs |
| 帧 CPU p95（每进程值的中位数） | 19,224 µs |
| `layout_and_synchronize` 外层 | 14,096 µs |
| 文本宿主 | 21 µs |
| 参与者几何同步 | 14,048 µs |
| Button / Gallery 参考 surface / Input / Selection 几何 | 27 / 42 / 13,967 / 12 µs |
| Input 文本图层 `TextSceneService::synchronize` | 13,940 µs |
| Input mounted 访问 / 文本图层同步调用 | 2,160 / 6,480 |
| Input 文本 geometry rebuild / patch / instance rebuild | 6,480 / 0 / 27 |
| 总 `ordered_scene_rebuilds`（含首次挂载） | 7,179 |
| Node / Component / Interaction | 923 / 923 / 61 |
| 滚动累计访问 Node | 222,443 |

每帧有 9 个 Input、每个三个文本图层。滚动改变 Input viewport 的全局位置；`InputComponentHost::synchronize_auxiliary_geometry` 把此位置写到 `GlyphPlacement.origin_pixels`，使 `TextSceneService::update_placement` 将三个图层标记为 position geometry dirty，逐层 `replace_text` 并重建 ordered scene。改造前的 6,480 次滚动期调用全部走 geometry rebuild，未走已有的 scroll translation geometry patch 路径。该因果关系由源码路径与计数共同支持；GPU 实际执行与像素等价尚未测。

telemetry 在自动滚动开始时清零，只有 `--scroll-acceptance` 开启内部细分计时。CPU 时间是墙钟阶段计时，包含计时器自身的小开销；比较前后时保持同一套计时器和字段。`ordered_scene_rebuilds` 是 TextSceneService 生命周期累计值，包含首次挂载及非 Input 场景，不当作滚动期纯计数。
