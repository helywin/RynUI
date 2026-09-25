# 命中批量刷新 CPU 基线

改造前运行时代码：`ff116f86a15bf18596493a199ca4cf65130c0c54`，另加入本 change 的 `tests/hit_test_dirty_refresh_benchmark.cpp` 与 CMake target；`HitTestSnapshot::refresh` 尚未修改。Windows 11 10.0.26200、Intel Core Ultra 9 285HX、MSVC 19.51.36248.0；正式 `windows-msvc` / Ninja Multi-Config / Release preset。五次独立进程，每次场景与 dirty 顺序固定；原始每次结果见 `benchmark-before.csv`。

| Node 数 | 交互记录数 | 不同 dirty Node 数 | 每轮刷新记录数 | 改造前 CPU ns/refresh 中位数 |
| ---: | ---: | ---: | ---: | ---: |
| 4,096 | 256 | 1 | 1 | 1,999 |
| 4,096 | 256 | 64 | 4 | 114,056 |
| 4,096 | 256 | 512 | 32 | 858,536 |
| 16,384 | 1,024 | 4,096 | 256 | 25,658,810 |

每场景预热五次后计时；计时只包含 `HitTestSnapshot::refresh`，不含节点/交互创建与快照 `rebuild`。场景节点均为一个根节点的直接子节点，交互记录无 parent，以隔离影响判断；dirty 用固定奇数步长遍历不同 slot。CPU 时间为进程内多轮平均，再取五进程中位数。该测试不代表 Gallery 的完整嵌套交互、SDL 上传、GPU 执行或展示延迟。`rynui.hit_test_dirty_refresh_benchmark` 在 Windows MSVC Release CTest 通过。

## Windows Gallery 真实窗口基线

运行时代码为 `4a205856218c9abf3fdfed42b376a1692b6af246`，外加本 change 的命中刷新计时与规模 telemetry，尚未修改刷新算法。Windows 11 10.0.26200、Intel Core Ultra 9 285HX、正式 `windows-msvc` / Release 干净构建，SDL 实际后端为 D3D12 / DXIL。`rynui_token_gallery.exe --scroll-acceptance` 连续五个独立进程均以 `exit_code=0` 完成 240 个自动滚动步骤；原始字段见 `gallery-before.csv`，完整程序输出保存在被忽略的本地 `out/build/windows-msvc/gallery-before-*.txt`。

| 指标 | 五进程中位数 |
| --- | ---: |
| Node / Component / Interaction 数 | 923 / 923 / 61 |
| 滚动期间累计访问的 Node 数 | 222,443 |
| 末帧考虑 / 可见 fragment | 886 / 87 |
| 平均整帧 CPU | 16,798 µs |
| 帧 CPU p95（每进程值的中位数） | 17,620 µs |
| 平均布局与场景同步阶段 CPU | 14,046 µs |
| 平均命中刷新 CPU | 5 µs |
| 平均资源同步 / 提交 CPU | 1,516 / 1,125 µs |
| GPU 上传提交 / Quad、Glyph、Effect draw 累计数 | 1,320 / 2,190、2,178、553 |

`frame_layout_us` 是 `layout_and_synchronize` 调用的外层耗时，包括文本、参与者、effect、fragment 和命中同步，不等于纯布局时间；命中刷新本身只占约 5 µs。这个窗口只有 61 条交互，且滚动时 dirty 集合可能较早包含公共祖先，因此本 change 的批量算法优化不能预期改善 Gallery 整帧。表中时间是 CPU 墙钟阶段计时；swapchain 等待包含于提交阶段，GPU 实际执行、输入到展示延迟均未测。

## 定向刷新改造后复测

改造后代码 `ebe592bc9dd22d336b69f5b6eaa5ce32ee8490ad`，同机、同一 Windows MSVC Release preset、`--clean-first` 构建、相同场景及五次独立进程；原始结果见 `benchmark-after.csv`。

| Node 数 | 交互记录数 | dirty 数 | 改造前 CPU ns/refresh 中位数 | 改造后 CPU ns/refresh 中位数 |
| ---: | ---: | ---: | ---: | ---: |
| 4,096 | 256 | 1 | 1,999 | 2,036 |
| 4,096 | 256 | 64 | 114,056 | 2,106 |
| 4,096 | 256 | 512 | 858,536 | 2,763 |
| 16,384 | 1,024 | 4,096 | 25,658,810 | 15,300 |

大场景的批量影响判断与刷新由约 25.66 ms 降到 15.3 µs，约 1,677 倍；单 dirty 从 1,999 到 2,036 ns，差值约 37 ns，不足以证明小批量性能变化。此处只测 CPU 命中刷新；Gallery 的实际整帧结果仍需独立复测。Release 定向 CTest 5/5 通过，包含命中、刷新、原有命中查询基准、新批量基准和 256 Input 稳态零分配验收。

## Windows Gallery 真实窗口复测

改造后代码 `ebe592bc9dd22d336b69f5b6eaa5ce32ee8490ad`，同机、同一正式 Windows MSVC Release preset，干净构建实际 D3D12 / DXIL Gallery。`--scroll-acceptance` 五个独立进程各完成 240 步并以 `exit_code=0` 退出；原始数值见 `gallery-after.csv`，完整输出保存在被忽略的 `out/build/windows-msvc/gallery-after-*.txt`。

| 指标 | 改造前五进程中位数 | 改造后五进程中位数 |
| --- | ---: | ---: |
| 平均整帧 CPU | 16,798 µs | 17,272 µs |
| 帧 CPU p95（每进程值的中位数） | 17,620 µs | 18,906 µs |
| 平均布局与场景同步阶段 CPU | 14,046 µs | 14,066 µs |
| 平均命中刷新 CPU | 5 µs | 5 µs |
| 平均资源同步 / 提交 CPU | 1,516 / 1,125 µs | 1,767 / 1,412 µs |
| Node / Component / Interaction 数 | 923 / 923 / 61 | 923 / 923 / 61 |
| 滚动累计访问 Node 数 | 222,443 | 222,443 |
| 末帧考虑 / 可见 fragment | 886 / 87 | 886 / 87 |
| GPU 上传提交 / Quad、Glyph、Effect draw 累计数 | 1,320 / 2,190、2,178、553 | 1,320 / 2,190、2,178、553 |

整帧中位数增加 474 µs，落在五进程波动范围内，不能归因于本算法；命中刷新保持 5 µs。Gallery 规模仅 61 条交互，因此定向批量基准的收益没有转化为该场景的整帧收益。自动滚动路径和退出码验证了此场景可运行，未进行人工逐帧视觉检查；GPU 实际执行和输入到展示延迟仍未测。当前可见的约 14 ms 布局与场景同步外层耗时需要单独拆分后再优化。
