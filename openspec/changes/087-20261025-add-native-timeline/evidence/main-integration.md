# 087 与冻结组件的父分支集成

2026-10-25；ROOT `D:/code/RynUI`，显式合并双亲为已发布 `ddb8a120ce780255759bf7981bd237cc231e9ec2` 与 worker `23545c02582234058d79dadcba1cffeaa105560b`。用户要求只完成现有 084/085/086/087 并停止，本阶段没有增加其他组件、change、PR 或 archive。

## 独立复核与合并语义

Timeline 组件 R1 的三项 P2 由 worker 先失败后修复：普通 document 富 Button fill 分区裁剪、loading rail 邻项、超过 4096 的虚线分批；独立 Astra R2 CLEAN，既有程序 Debug/Release 各 15/15。实际 5001/5001、缩小 11/11 的轨迹与清理证据见 `astra-r1-fixes.md`。这不是父组合的新构建证明。

共享合并保留 Timeline 的 document roots/document clip 和逐记录转换清理，窗口层 admission 使用 notices 已修正的 `in_window_layer`；窗口启用条件为 Modal 或 notice 可见，document roots 独立生效。每条 surface/content-range 失活时清理转换缓存并恢复原 draw/effects，没有用全局关闭分支清除仍有效的 Timeline。

组合 Astra R1 发现一项新 P2：notice 使全局 hit clip 扩大，导致 document clip 外的不可见按钮仍可命中。新增真实 Timeline Button/Notification 回归，在未改生产代码时 Debug/Release 均准确 exit 1：`visible notice exposed an invisible document Button to pointer input`。修复把 document clip 与普通 interaction 的已有 explicit clip 求交，窗口层仍独立；HitTest 保存 source clip，WindowComponentServices 同时缓存全局与 document clip，扩缩文档可立即重建。新测试还验证浮层按钮在 document clip 外仍可点击、隐藏按钮不触发、两轮恢复。

全新 Astra 组合 R2 对预期 dirty merge 源码判 **CLEAN**，追踪 explicit/source clip、局部 refresh、clip_boundary、两种 clip 缓存、转换清理与源 draw 重新绑定；它没有构建或执行程序，不冒充运行证据。

## ROOT 实际新构建与执行

正式 MSVC、Ninja Multi-Config，最多 4 jobs。Debug 使用 `windows-msvc-debug`；Release 使用 `windows-msvc-headless-release`（HEADLESS/RECORDING）。

```powershell
./out/timeline-parent-msvc.ps1 -Stage build -RunId r3
./out/timeline-parent-msvc.ps1 -Stage build -RunId r4
./out/timeline-parent-msvc.ps1 -Stage goldens -RunId r4
./out/timeline-parent-msvc.ps1 -Stage tests -RunId r4
./out/timeline-parent-release.ps1 -Stage build -RunId r2
./out/timeline-parent-release.ps1 -Stage tests -RunId r2
```

全部 exit 0。Debug 去重 **96/96，18.17 秒**；Release 去重 **53/53，8.56 秒**。包含全部 notice、Timeline、相关 Popover 复核/组合、Descriptions clips、Theme、Modal/selection/scene；Debug 另含 Date/Time/reset、Table/Select/输入和八项 Gallery contracts，新增 SceneComposer/HitTest/refresh/registry 原有检查；Release 含 portable backend boundaries。这里没有运行历史全套、完整 frame 或重复 journey。

修复前 Debug 91/91 是历史的缺场景结果，不能代替新回归。失败复现日志 `out/087-document-hit-{debug,release}-before.log`；初次夹具编译因 TimelineItem 缺 content 参数失败的日志也保留，补齐公开构造参数后才取得上述真实失败。修复后 `document_hit` 在最终两配置 CTest 均通过；原 `fill` 用例保留原断言，并增加三轮消息开关期间 Timeline 实际 coverage、身份和资源回基线检查。

## Theme、架构与检查

Debug/Release 新程序各生成五份 golden，文件字节完全一致；命中修复前后亦一致。双向深比较保留 ddb8 的所有旧字段类型和值（只追加 Timeline/identity），以及 2354 的旧字段（仅追加 Popover/Message/Notification/identity）。原 **35** 个父 IIFE、worker **34** 个 IIFE 原文和顺序保留；合并为 36。已发布 **194** 个 TokenIdentity 前缀不变，Timeline 两项追加其后。

Popover 父子定位/Escape、Date/Time 同值 Form reset 唯一 hook、静态默认 Theme、heap descendant preflight/self guard/Binding 桥均保留。Debug/Release 的 notice/timeline/popover review 六个 PE 程序实际检查仍为 **1 MiB** stack reserve；没有扩大默认栈或修改 renderer ABI。

clang-format 22.1.3：**723 文件、0 failures**；OpenSpec 1.14 doctor healthy、strict **87/87**；catalog generator/check、自有源码与 staged diff checks 通过。原始执行日志 `out/087-main-r4-*`、`out/087-headless-release-r2-*`，旧失败/先前结果均单独保留。

## 验收边界

本记录证明冻结功能组合的新构建和平台通用合同。最终完整 Gallery 与 Windows 原生启动将在本阶段发布后固定源码 SHA 各执行一次，另记实际日志与范围；不把本记录升级为组件完整 GPU/人工视觉、Linux、系统 IME 或 AT 验收。只推送 main，保留工作树历史，不推送 t3 分支。

## 后续统一验收

本阶段发布的 `fbce29a0` 已完成一次完整 Gallery 9/9 及独立 Windows D3D12/DXIL 七帧启动，见 [批次最终记录](../../086-20261025-add-descriptions-and-skeleton-gallery/evidence/03-frozen-batch-final.md)。这些是完整 Gallery 和启动证据，不补称 Timeline 的专属 Gallery/GPU/人工视觉验收。
