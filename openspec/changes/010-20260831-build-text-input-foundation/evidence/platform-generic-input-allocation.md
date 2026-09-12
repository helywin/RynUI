# 平台通用：Input retained 更新分配与回归

任务 6.5、6.6；2026-09-12，Windows/MSVC，正式 `windows-msvc-debug` preset。

## 负载与断言

`rynui.input_scene_allocation` 使用真实 FontRuntime、Input、Text、布局、retained scene 及 Quad/Glyph/RoundedEffect GPU resource 类，每种模式挂载 256 个 Input 和一个无关 Text。普通 selection 和内容不变的 composition-selection 分别预热 20 次，再各运行 20,000 次更新。

- 通过全局 C++ new/new[] probe 检查 dispatch 与同步热路径总堆分配为 0，包括 MSVC Debug iterator proxy。
- 所有 256 个 Input 的 shaping、measure、placement 计数不增长；无关 Text 不重新 shaping。
- Component 数量、mount 次数、composer rebuild、glyph 数量、Quad capacity、effect slot capacity 和 HitTest refresh 计数不增长。
- 执行正式 GPU resource 同步，上传范围不能逃出目标 Input 的 selected glyph range 和三个 Quad；预热后不上传 atlas 或圆角效果。
- 修复成功同步反复构造空错误字符串、Text fragment 临时命令数组、glyph dirty range 合并和 GPU dirty range 临时数组造成的分配；复用 retained storage，不关闭失败重试合同。

## 实际结果

- Debug 构建成功。包含 GPU 同步的全量 CTest 195/195 通过，总耗时 273.11 秒；其中双模式 allocation benchmark 157.55 秒。
- 随后补充全部 256 个 Input 的 shaping/measure/placement 以及 Component count 断言，重新构建并运行 TextCaretMap、Input display/component/GPU/allocation、Glyph scene、Text scene/component/frame，共 9/9 通过，总耗时 190.84 秒；双模式 allocation benchmark 186.82 秒。
- 两种模式均执行 20,000 次更新并满足零分配及上述稳定性断言。耗时仅记录本机 Debug 测试，不作为帧率目标或跨机器性能结论。

GPU 接口由无分配 counting sink 实现；这是正式资源类的接口级验证，不是原生驱动内存、实际 GPU 画面或 Windows/Linux IME 验收。原生平台项目继续独立保留，3.7 仍为用户批准的非阻塞待修复项。
