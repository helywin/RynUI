# Reactive Quad update-loop validation

验证日期：2026-08-26

本阶段在 Windows 真实窗口中验证 `Signal -> Binding -> Node property -> Dirty queue -> Quad instance range -> SDL GPU` 更新闭环。示例依次触发一次 Material 批量更新、一次 Transform 更新和一次 size 更新；Component 仅在 mount 时执行，稳定状态通过阻塞式事件等待保持空闲。

| Update | Signal writes | Observer executions | Layout | Quad range upload | Visual result |
| --- | ---: | ---: | --- | --- | --- |
| initial mount | 0 | 4 | 初次 Measure/Place | 48 bytes initial upload | 蓝色圆角 Quad |
| Material: color + opacity | 2 | 2 | 不触发 | 48 bytes | 紫色且透明度降低 |
| Transform: translation | 1 | 1 | 不触发 | 48 bytes | Quad 向右下移动 |
| size | 1 | 1 | 受影响根重新 Measure/Place | 48 bytes | Quad 扩展为最终尺寸 |

Windows Graphics Capture 对最终窗口的核对结果为：深色背景上的紫色圆角 Quad，边缘完整，透明度混合正常，更新后尺寸和位置符合预期。

| Preset | Configuration | GPU driver | Shader format | CTest | Exit | Counters |
| --- | --- | --- | --- | --- | ---: | --- |
| `windows-msvc` | Debug | `direct3d12` | `DXIL` | 24/24 PASS | 0 | `component_runs=1 signal_writes=4 observer_executions=8 measure=4 layout=4 primitive_rebuilds=1 instance_updates=3 gpu_uploads=4 gpu_uploaded_bytes=192 submits=4 idle_wakes=0 idle_waits=139` |
| `windows-msvc` | Release | `direct3d12` | `DXIL` | 24/24 PASS | 0 | `component_runs=1 signal_writes=4 observer_executions=8 measure=4 layout=4 primitive_rebuilds=1 instance_updates=3 gpu_uploads=4 gpu_uploaded_bytes=192 submits=4 idle_wakes=0 idle_waits=139` |

初次 upload 与三次精确 range upload 均为单个 48-byte instance，总量为 192 bytes。四次 submit 分别对应初始帧和三组状态更新；之后的 139 次 idle wait 没有产生额外 submit，证明稳定状态不会按刷新率持续提交。
