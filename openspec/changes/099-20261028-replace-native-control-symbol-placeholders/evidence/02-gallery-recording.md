# Gallery Recording 复验

Windows MSVC / Ninja Multi-Config，Release，最多 2 jobs。图标生产源码为 ac0d1196；本阶段仅补齐 DatePicker/TimePicker 独立导航期望中的既有 TreeSelect 锚点，不改生产导航或交互断言。

三目标实际构建成功；最终 frame TU 编译与链接 18.8602047 s。首次 DatePicker 失败为旧期望漏 TreeSelect，诊断日志保留。

最终同一 frame 程序：common 6.6058399 s、data 7.2976451 s、date-picker 46.7204285 s、time-picker 63.5943277 s，全部 exit 0。精确 inventory 6.216372 s、8 项 contracts 0.4086688 s，全部通过。

实测保持 458 IDs、93 navigation、433 Buttons、1593 interactions、771 visible、307 live、163 Inputs、5 Forms/8 items、2 DatePicker/2 TimePicker、3 Trees/2 TreeSelect。固定 Icon slot 未增加交互数量；普通页码/日期与原操作仍经回归验证。

原始日志及结果见相邻 `stage2-artifacts`。这些是新构建 Release 的 Recording 验收；不代替 2.1 实际窗口图标、命中和焦点验证。
