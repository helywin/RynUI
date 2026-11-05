# Checkbox 空 label 检查：Linux

以下为来源提交 `f8ed1d95a8af469fede943d5deafddca2363cffa` 的独立工作树记录；当前 main 重新验证见 [共享基础集成](shared-runtime-primitives-integration.md)。耗时对照仅属于来源诊断。

- 唯一行为优化：仅存在 `CheckboxLabel` 时遍历交互后代进行被动内容验证；原有拒绝逻辑完整保留，Radio 策略未改动。
- `linux-gcc-headless` / Ninja Multi-Config / `-j2`，Debug/Release 的 `rynui.selection_component` 各 **1/1** 通过，保留旧断言并新增 Text/Icon、Button/Input/嵌套交互拒绝及清理、旁边 1000 个无关交互的无 label 合同。
- 诊断对照使用同一 Debug 对象/依赖集合，仅移除 guard 重编 Selection 源文件；所有源码副本、diff、二进制和日志在忽略的 `out/`。测试载体为本次 Table 实施工作树，数据/字体/断言相同，后台有 `-j2` 构建负载；不设耗时通过阈值。
- 非虚拟、带 rowSelection，窗口 640×600、scrollY=180。1k/5k/10k 实际保留 1k/5k/10k 行、2k/10k/20k 格。case CPU 秒（含挂载和原更新断言，不含 case 后析构）：无 guard **1.950/45.563/210.044**，有 guard **1.495/29.084/155.716**；两侧 **3/3** 通过。这不作为虚拟窗口或主线最终性能证据。
- clang-format 22.1.3、doctor healthy、strict **107/107**、diff check 通过；无平台/GPU/公开 API 变更。
