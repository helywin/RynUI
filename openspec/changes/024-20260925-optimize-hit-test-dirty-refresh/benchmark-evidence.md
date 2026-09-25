# 命中批量刷新 CPU 基线

改造前运行时代码：`ff116f86a15bf18596493a199ca4cf65130c0c54`，另加入本 change 的 `tests/hit_test_dirty_refresh_benchmark.cpp` 与 CMake target；`HitTestSnapshot::refresh` 尚未修改。Windows 11 10.0.26200、Intel Core Ultra 9 285HX、MSVC 19.51.36248.0；正式 `windows-msvc` / Ninja Multi-Config / Release preset。五次独立进程，每次场景与 dirty 顺序固定；原始每次结果见 `benchmark-before.csv`。

| Node 数 | 交互记录数 | 不同 dirty Node 数 | 每轮刷新记录数 | 改造前 CPU ns/refresh 中位数 |
| ---: | ---: | ---: | ---: | ---: |
| 4,096 | 256 | 1 | 1 | 1,999 |
| 4,096 | 256 | 64 | 4 | 114,056 |
| 4,096 | 256 | 512 | 32 | 858,536 |
| 16,384 | 1,024 | 4,096 | 256 | 25,658,810 |

每场景预热五次后计时；计时只包含 `HitTestSnapshot::refresh`，不含节点/交互创建与快照 `rebuild`。场景节点均为一个根节点的直接子节点，交互记录无 parent，以隔离影响判断；dirty 用固定奇数步长遍历不同 slot。CPU 时间为进程内多轮平均，再取五进程中位数。该测试不代表 Gallery 的完整嵌套交互、SDL 上传、GPU 执行或展示延迟。`rynui.hit_test_dirty_refresh_benchmark` 在 Windows MSVC Release CTest 通过。
