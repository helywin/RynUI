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

## 改造后复测

改造后代码：`bec2fe8`。使用同一机器、`windows-msvc` / Release、相同场景及五个独立进程；原始记录见 `benchmark-after.csv`。下表仍为 CPU 微秒中位数。

| N | D | dirty 入队 | Quad 收集 | Quad 规划 | atlas 命中查找 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1,024 | 256 | 11 | 12 | 1 | 2 |
| 4,096 | 256 | 15 | 4 | 0 | 4 |
| 4,096 | 1,024 | 41 | 17 | 2 | 8 |
| 16,384 | 256 | 19 | 7 | 0 | 4 |
| 16,384 | 4,096 | 206 | 70 | 12 | 43 |

在 N=16,384、D=4,096 的合成场景，dirty 入队约为基线的 1/6.4，Quad 收集加规划约为 1/294，atlas 命中查找约为 1/3,230。N=1,024、D=256 的 cold dirty 入队为 9→11 µs；微秒级差值不构成小场景 p95 已改善的证据。该 benchmark 只隔离三个 CPU 算法；不能将乘数外推到整帧、真实 GPU 或输入延迟。

最终代码在 Windows MSVC Release preset 做了 `--clean-first` 全构建。全量 CTest 为 226/232，通过项包含 dirty queue、Quad/Glyph、atlas、Input pointer/keyboard 零分配、场景与 renderer 测试。余下 6 项失败分别属于既有 Ant Design catalog/证据生成、Theme golden 和 dependency lock SHA 校验，本 change 未修改这些文件；排除这 6 项后 226/226 通过。旧本地 CMake 配置的 MSVC `/showIncludes` 前缀乱码曾使头文件改动不触发增量重编译，因此最终采用干净构建，测试环境由 VS Developer Command Prompt 提供 MSVC、Ninja、Windows SDK 工具；这属于构建环境边界，不计作运行时性能收益。
