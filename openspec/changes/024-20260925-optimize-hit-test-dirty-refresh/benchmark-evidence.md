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
