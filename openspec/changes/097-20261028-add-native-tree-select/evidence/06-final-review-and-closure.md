# 097 最终复核与完成记录

097 首版 **5/5 tasks 完成**。最终生产源码为 `1eeec400856a078824a8f97df8f74bd5f4bed461`，直接 parent `cc006b77a9f5d6cb6e5accf06fb0e41006ff1466`；提交后实际 push main 并通过 ls-remote 核对同一完整 SHA。此次收口只更新文档与任务状态，不再修改生产、测试、Theme 或金样。

## 独立 Astra R4

配置起止读回均为 `gpt-6-astra / xhigh / serviceTier=default`。审查固定 ROOT main 上述完整 SHA，前后 porcelain 为空，未访问远端或改动文件。

结论 **CLEAN**：完整审阅从 `3e49f7e3` 开始的 097 首版 API/model/host、Theme、构建注册、测试、规划与 evidence 01–05；未发现可达 P1/P2，R1/R2/R3 六项 finding 均闭合，约定首版范围实现完整。R3 的显式帧唤醒、关闭／拒绝／异常后意图清理及回调代际检查成立，未发现新增 UAF 或 idle busy loop。

本轮直接运行已有 HEADLESS EXE，Debug／Release 各 **23/23**，合计 **46/46 exit 0**：

- TreeSelect model/theme；component 的 basic、controlled、keyboard、retained、boundaries。
- integration 的 form、lifecycle、rollback、positioning、matrix。
- R1 outside_retry、disabled_entry；R2 focused_outside、suppressed_close；R3 keyboard_wake、cancelled_entry。
- Popover model、review focus/escape；portable pointer_route、focus_state。

R3 四种按键均 `pending=1`，四种取消路径均 `retained_selector=1 blurs=0`，停稳断言通过。审核未重新构建，以上属于 independent existing-binary 复跑，不是新一轮 fresh build。

## 父验收与保留性

父任务在同一生产源码上实际完成官方 Windows MSVC HEADLESS／Ninja Multi-Config 新构建，每配置重新链接 36 个指定目标，Debug／Release 各 **78/78**；精确命令、耗时、修复前失败和原始日志见 [R3 修复记录](05-astra-r3-fixes.md)。格式检查 **803 文件、0 failures**，显式 OpenSpec **1.14.0** doctor healthy、strict **97/97**，diff check 通过。

R4 核对 [R3 索引](r3-artifacts/index.json) 的 28 份记录、3 个 source/CMake 指纹、72 个 PE、4 个 OBJ、2 个 LIB 和五份金样一致，源码→OBJ→LIB/EXE 时间链成立，Ninja 摘录逐行对应原日志；PE 默认栈均 **1 MiB**。同时核对父 fresh-build 日志与 78/78 结果，未将其冒充审核自己的新构建。

41 个旧 IIFE、209 ordinal、17,612 typed leaves 与 129 冻结文件保持，四处精确例外未扩大；R1/R2 测试原文不变，五份金样与 parent 字节一致。静默退役、RC／系统字体、Core/renderer 边界与旧组件保持。

## 完成边界

完成的是 097 约定的原生单选 TreeSelect 与平台通用验证。多选／checkbox、搜索、动态拓扑、异步、富节点／自定义 popup、虚拟化、动画、RTL/Web/AT 和交互 Gallery 不属于此首版。未完成的分平台行为不据此勾选：本记录不提供真实窗口／GPU、Linux、系统 IME 或辅助技术验收，也不沿用旧批次 Gallery 结果。
