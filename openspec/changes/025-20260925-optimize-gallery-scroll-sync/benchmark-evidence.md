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

## 改造后五进程复测

代码 `3a7a08b` 及相同 telemetry；同机、同一正式 Windows MSVC Release `--clean-first` 构建，实际 D3D12 / DXIL。每个进程完成 240 个自动滚动步骤且 `exit_code=0`，原始字段见 `gallery-profile-after.csv`，完整输出保存在被忽略的 `out/build/windows-msvc/gallery-profile-after-*.txt`。

| 指标 | 改造前中位数 | 改造后中位数 |
| --- | ---: | ---: |
| 240 个滚动帧的平均整帧 CPU | 17,339 µs | 4,134 µs |
| 帧 CPU p95（每进程值的中位数） | 19,224 µs | 5,123 µs |
| `layout_and_synchronize` 外层 | 14,096 µs | 122 µs |
| Input 文本图层同步 | 13,940 µs | 2 µs |
| 240 步滚动完成墙钟时间 | 4,164 ms | 994 ms |
| Input 文本 geometry rebuild / patch | 6,480 / 0 | 0 / 6,480 |
| `ordered_scene_rebuilds`（含首次挂载） | 7,179 | 699 |
| Input 文本同步调用 | 6,480 | 6,480 |
| GPU 上传提交 | 1,320 | 1,320 |
| Quad / Glyph / Effect draw | 2,190 / 2,178 / 553 | 2,190 / 2,178 / 553 |
| Node / Component / Interaction | 923 / 923 / 61 | 923 / 923 / 61 |

优化后 240 步约一秒完成，而旧实现约四秒；Gallery 原有验收仍运行到启动后 1.8 秒，快路径会在滚动结束后额外提交动画帧。因此最终 telemetry 固定捕获开始后的前 240 个提交帧，并在第 240 帧保存 renderer 计数；旧基线本来恰好捕获 240 帧，前后比较才使用同一工作窗口。滚动仍以原有 240 步、最终 offset、可见 fragment 与退出码验收。平均整帧 CPU 下降约 76%，p95 下降约 73%；Submit 阶段等待在快路径中上升，不能把阶段 CPU 墙钟时间解读为 GPU 执行时间或输入到展示延迟。

新路径只改变 Input 文本图层的平移表达：整物理像素部分通过 post-raster geometry patch，分数残差继续参与 glyph raster phase。输入水平滚动与容器平移一次合并写入，避免同帧重复脏化；对照完整重建的多 DPI/clip/离屏内容回归以及 Windows Debug 定向测试通过。自动滚动验证了真实窗口可运行及终态统计，未做逐帧人工像素检查，GPU 实际执行仍未测。

## 集成校验状态

Windows MSVC Release 完整 `--clean-first` 构建成功，完整 CTest 227/233 通过。六项失败与 024 集成基线一致：`rynui.design_token_catalog`、`rynui.ant_design_current_baseline`、`rynui.ant_design_665_evidence_contract`、`rynui.ant_design_gallery_catalog_generator`、`rynui.theme_algorithm`、`rynui.dependency_lock`；本 change 相关 Input、文本场景及 Gallery 测试通过。完整原始输出在被忽略的 `out/build/windows-msvc/gallery-scroll-full-ctest.log`。额外的真实 D3D12 窗口 `--input-acceptance`、`--selection-acceptance`、`--password-acceptance`、`--input-clear-acceptance` 均以退出码 0 完成。

`openspec validate 025-20260925-optimize-gallery-scroll-sync --strict --no-interactive` 通过；全仓 strict validate 19/25 通过，既有 change 013、015、016、017、018、021 缺 delta spec。OpenSpec CLI 1.4.1 的 `openspec doctor --json` 返回 `unknown command 'doctor'`。`git diff --check` 通过。因此全仓集成门槛尚未满足，tasks 3.1 保持未勾选。
