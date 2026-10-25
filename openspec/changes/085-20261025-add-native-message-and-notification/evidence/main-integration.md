# 085 父分支集成记录

2026-10-25，ROOT `D:/code/RynUI`；集成双亲为已发布 `b5728c17413801d2ae07be79cf981971e8dc6ba5` 与 worker `9b0e59b026072eb31837145affb4714f64bf2fe5`。此阶段没有导入 Timeline；最终冻结组合的完整 Gallery/native 启动另行记录。

## 实现与审查

Message/Notification 为窗口内真实 holder/ref 实现，包含 keyed retained 更新、真实 slots/控件、数量限制、自动关闭与生命周期清理。首版排除项见 `docs/message-notification.md`，没有用目录状态替代实现。

独立 Astra R1 的填充裁剪、重入淘汰问题由 `d25c3ef4` 修复；R2 的最后 notice 关闭后转换缓存恢复问题由 `9b0e59b0` 修复。修复前失败与 Debug/Release 结果见同目录 `astra-r1-fixes.md`、`astra-r2-fix.md`。R3 固定该 HEAD 为 CLEAN，实际复验既有程序 Debug 16/16、Release 16/16；这不代替本次父分支新构建。

## ROOT 实际执行

正式 `windows-msvc-debug`，MSVC、Ninja Multi-Config，最多 4 jobs。`out/notices-parent-msvc.ps1` 的 33 个目标包括 Gallery/frame/catalog、全部 notice 与 Popover 测试及相关 Theme、Date/Time、输入、共享裁剪程序。

首次组合构建准确暴露 notice Theme 测试仍假定 identity 紧跟 Descriptions 的静态断言。生产 ordinal 保留已发布 Popover 前缀，将该断言改为接在 `popover_order` 后；保留失败日志 `out/085-main-r1-build.log`。随后依次执行：

```powershell
./out/notices-parent-msvc.ps1 -Stage build -RunId r2
./out/notices-parent-msvc.ps1 -Stage goldens -RunId r2
python out/check-date-picker-integration.py b5728c17413801d2ae07be79cf981971e8dc6ba5 message,notification
python out/check-date-picker-integration.py 9b0e59b026072eb31837145affb4714f64bf2fe5 popover
./out/notices-parent-msvc.ps1 -Stage tests -RunId r2
./out/notices-parent-msvc.ps1 -Stage inventory -RunId r2
```

全部 exit 0。Debug 去重 CTest **81/81，13.48 秒**；包括 notice 的所有注册用例、R1/R2/anchor 回归、Popover 复核、Date/Time/Form reset、共享输入与八项 Gallery 合同。没有运行完整 frame 或重复 journey。新生成五份 golden 对两个双亲保留旧字段类型和值（仅允许 identity 与另一方新组件）；33 个已发布 IIFE 与 worker 35 个 IIFE 均原文、原序保留。TokenIdentity 保留已发布前缀，Message/Notification 八项追加其后。

实际 inventory：358 IDs、93 navigation、367 Buttons、1412 interactions、671 visible、291 live、151 Inputs、24 Select、3 Forms/6 items、2 DatePicker、2 TimePicker、2 Breadcrumb、1 Steps、1 Descriptions、4 Skeleton。新通知没有额外 Gallery 样例。

clang-format 22.1.3：**705 文件、0 failures**；OpenSpec 1.14 doctor healthy、strict **86/86**；catalog generator 与 diff checks 通过。构建与执行原始日志为 `out/085-main-r2-*`，最低检查为 `out/085-main-{format,doctor,strict}.*`。

## 保留与边界

Theme、宿主、测试接入按双方并集处理；Popover 的双向父子定位、trigger Escape 与默认栈修复保留。notice 宿主在 Modal/锚定 popup 之前；内容处理后再执行所有边界兜底。此阶段采用 worker 的转换关闭清理，后续 Timeline 合并必须同时保留普通 document 根范围与逐记录清理。

上述结果是 Windows 上平台通用逻辑及 Recording 检查。未在此阶段新增完整 Gallery/native、组件 GPU/人工视觉、Linux、系统 IME 或 AT 验收。最终统一运行后单独补充，不复用历史验收作为当前源码证明。

## 后续统一验收

与 087 合并及 document hit 修复后的冻结源码 `fbce29a0`，已完成完整 Gallery 9/9 和独立 Windows 启动，见 [批次最终记录](../../086-20261025-add-descriptions-and-skeleton-gallery/evidence/03-frozen-batch-final.md)。这里的 81/81 仍是通知阶段的历史组合结果，不替代最后的 96/96、53/53。
