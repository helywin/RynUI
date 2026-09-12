# 平台通用：Input Token override 与订阅子阶段

2026-09-12，任务 7.1 的尺寸 Token 子集继续实施；状态颜色/阴影尚未接入，本项不勾选完整 7.1。

- `ThemeConfig.input` 增加 typed LogicalLength 字体、三档横纵 padding、middle radius 和 affix gap overrides，以及隔离的 component seed/algorithm 开关。
- 已解析的 `InputTokenSet` 保持内部类型；ThemeSnapshot 用 immutable shared ownership 保存它，计入 equality、identity 与诊断 JSON。没有 SDL 或 renderer 类型泄漏。
- nested override 保留父级未覆盖字段；默认 small font 随 base font 更新，显式 small font 不被覆盖；自动 block padding 随字体变化重算，继承的显式 block padding 保留。
- 新增 `Input.layoutMetrics`、`Input.typography`、`Input.borderRadius` 独立 typed 订阅。padding 只触发布局相关阶段，字体触发必要 shaping，radius 只触发 geometry/material。
- 验证 algorithm=true 的 component seed 不改变全局/Button，algorithm=false 不消费其 component seed，非法长度原子拒绝且不发布 snapshot/notification。
- Default/Dark/Compact 及两种 dark/compact 顺序的五份 golden 已更新；程序化比较确认原有 source、algorithm、seed、map、alias、Button、Text 字段全部不变，只新增 Input 与更新 identity。
- 用锁定 `740ad964dc2397f33e40944367b0536a7314cc32` source checkout 运行生成器的 write、verify-source、verify-repository、self-test，全部成功。仅将已实现的九个 Input padding/font Token 提升为 runtime；activeShadow 等状态 Token 仍为未实现。

Windows/MSVC `windows-msvc-debug` 构建成功；新增 override、订阅、atomic failure、实际 Input targeted reshape 加入后，Theme/Input 相关 CTest 11/11 通过（4.09 秒）。随后包含生成 metadata 新断言和溢出保护的全量 CTest 196/196 通过（237.35 秒），其中 256 Input、两种各 20,000 次更新的零分配基准通过（121.94 秒）。这不是原生窗口验收。
