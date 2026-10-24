# Design

## Context

Input 支持真实编辑/IME、Button 支持原生激活，已有 Select/Menu/Table 和窗口层拥有保留插槽、滚动及边界处理。077 DatePicker 已实现但在独立分支修复首轮 Astra 问题，不能把其未合入代码视为 main 已有能力。TimePicker 只选择一天内的时间，不依赖公历或第三方日期库。

已读本地锁定的 Ant Design 6.6.5 time-picker 文档；上游提供步长、禁用、needConfirm、footer、24/12 小时制等。首版范围见 proposal/spec，不把暂未实现的上游能力计入完成。

## Goals / Non-Goals

**Goals:** 完成真实输入→打开面板→调整各列→确认/取消的可用闭环；共享现有生命周期、Theme 与输入层合同，避免新增通用 picker 框架。

**Non-Goals:** 不抽象 DatePicker/TimePicker 的所有差异，不实现新的文字编辑器、日期时区系统、虚拟列表或组件私有 renderer 路径。Gallery 和平台矩阵独立推进。

## Decisions

1. 公开有效 `TimeOfDay` 与 `std::optional` 选择，model 独立处理严格解析/格式、步长、范围和禁用。24/60/60 的有限域提供显式搜索上限；无可用时间时不回退到非法值。程序化受控值保留 authority，不因步长默默重写；分钟精度显示省略秒，新提交统一零秒，并在文档说明外部值的显示规则。
2. host 保留真实 Input、时间列和 typed footer；输入草稿、待确认面板值、已接受值分别遵守 spec。默认确认使用真实 Button；即时模式列选择请求合法值但不关闭，以便连续调整。没有值时使用显式 defaultPanelValue 或 00:00:00，不读取系统时钟。
3. 列使用现有滚动/interaction/焦点和 retained surface。每列最多 60 项，不引入虚拟化；只有可见且未被裁剪的行可命中。键盘 reveal 与 wheel 路由处理真实可见列；边界兜底位于子内容处理之后，最近 popup ownership 防止穿透。
4. Theme 使用独立 component tokens 及完整 seed/algorithm/override/inherit/hash/dirty/serialization 链路。校验所有最终派生度量，忽略未启用算法的局部 seed；沿用 main 的子树发布前预校验和堆上大 snapshot 处理。五份 fresh goldens 对照合入基线保留旧字段，避免把仅生成成功当语义兼容。
5. 使用 CMake `windows-msvc-headless`、Ninja Multi-Config、MSVC 完成平台通用 Debug/Release 合同；实际组件/Theme/input/popup 回归验证。只有实际运行对应原生环境后才新增 GPU、系统 IME 或 Linux 验收，不由共同测试替代。

## Risks / Trade-offs

- [全禁用/动态规则导致搜索或重复提交] → 限定一天内域，测试全禁用、边界和回调销毁；去重请求与 suppression 分离。
- [footer 子 popup/滚动被父边界吞掉] → 内容先于边界兜底，覆盖真实 footer Table、Select、重叠兄弟及滚动边界。
- [Theme 结构继续增长触发 Windows 栈限制] → 保留 main 已验证的堆持有/分离调用，必要时对实际失败定位，不盲目增加线程栈。
- [首版能力被目录夸大] → docs 明列 24 小时、严格两种格式和未覆盖范围，尚未执行的 Gallery/平台项保持未完成。
