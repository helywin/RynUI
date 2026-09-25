# 局部更新 CPU 基线

基线代码：`775d76e3248cdb5261c74250c45704c98bd12f85` 加本 change 的 `tests/rendering_locality_benchmark.cpp` 与 CMake target；三处待优化实现尚未修改。平台为 Windows 11 专业工作站版 10.0.26200、Intel Core Ultra 9 285HX、MSVC 19.51.36248.0。配置为 `windows-msvc` / Ninja Multi-Config / Release。每组独立启动 5 次进程；下表取 5 次 CPU 微秒中位数，原始记录见 `benchmark-baseline.csv`。

| Node/实例/atlas entry 数 N | 不同脏对象数 D | dirty 入队 | Quad 收集 | Quad 规划 | atlas 命中查找 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1,024 | 256 | 9 | 82 | 0 | 533 |
| 4,096 | 256 | 15 | 75 | 0 | 2,131 |
| 4,096 | 1,024 | 123 | 1,341 | 0 | 8,552 |
| 16,384 | 256 | 21 | 78 | 0 | 8,538 |
| 16,384 | 4,096 | 1,317 | 24,138 | 0 | 138,979 |

`quad_plan_cpu_us` 为旧版 getter 的单次时间，微秒整数计时分辨率下为 0；旧版归并工作已计入 Quad 收集。benchmark 用空 glyph 避免字体 raster 与 atlas 页分配，并在 setup 后测命中；不代表真实 CJK 首次绘制。CPU 测时未包含 Node/instance/atlas setup、GPU 上传、绘制、交换链等待或展示延迟。GPU 时间：未测。`rynui.rendering_locality` CTest 在 Release 通过。
