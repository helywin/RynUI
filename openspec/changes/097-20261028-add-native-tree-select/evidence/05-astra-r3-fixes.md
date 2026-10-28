# Astra R3 键盘帧请求与待聚焦生命周期修复

基线：`cc006b77a9f5d6cb6e5accf06fb0e41006ff1466`。独立 R3 对该固定提交完成完整源码审查及 42/42 次 existing-binary 运行，确认 R1/R2 已闭合，但源码推演发现两项相邻 P2。本阶段在原生产基线上实际复现后修复；3.1 继续 unchecked，等待全新独立 R4。

## 实际复现

新增 `rynui_tree_select_r3_tests`，只链接原基线生产。`keyboard_wake` 使用已打开弹层和 keyboard modality 的 selector，先完成布局并消费旧帧请求，再发 Down。原结果 `pending=0`，没有新请求；测试在确认请求之前不会调用同步，避免原测试无条件 sync 掩盖缺口。

`cancelled_entry` 在打开回调内接受 true 再提交 false，尚未进入 Tree 就关闭。关闭同步后独立提交 true，原结果 `retained_selector=0 blurs=1`：旧意图恢复并夺走 selector 焦点。

```text
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r3-msvc.ps1 -Stage before-build -Configuration <Debug|Release>
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r3-msvc.ps1 -Stage before -Configuration <Debug|Release>
```

两个配置均 **0/2，CTest exit 8**。Debug real 0.17 s／wrapper 0.2986579 s；Release real 0.11 s／wrapper 0.2384149 s。每个 case 的扩展组合在首个失败后尚未运行，不声称全部内部组合在基线上动态失败。

最初测试探针误把返回 void 的 FocusManager::dispatch 接为 bool，Debug 编译失败；修正探针后才完成以上复现，生产当时未改。原编译错误与结果单独保留，未计为组件故障复现。

## 修复与扩展回归

- 生产只改 `tree_select_component.cpp`。当前 owner 完整代际仍有效、接受的 popup 已打开且键盘意图仍有效时，显式通过 `WindowComponentServices::request_frame()` 唤醒下一帧；不依赖无变化 Signal 或已经是 keyboard 的 modality 再次唤醒。
- 外部 open=false 立即清除待聚焦意图。真实 suppression 也清除；构建祖先的临时封锁保留既有区分。打开回调完成后仍拒绝／关闭时取消意图；refresh 或通知抛异常时清除原 shared state 的意图并重抛原异常。后续独立程序化打开不继承该轮键盘焦点请求。
- request 返回后先重查原 owner／私有 graph，再请求帧；回调退役或同 ref 新 generation 不续用旧组件。R1/R2 原测试与断言未修改。

最终 `keyboard_wake` 覆盖 Down／Up／Enter／Space，四种均 `pending=1`，确认请求后才执行 geometry，实际焦点进入可用行；完成后额外同步仍无帧请求／deadline。`cancelled_entry` 覆盖回调内 true→false、回调接受后首次 geometry 前外部关闭、无 authority 变化的直接拒绝，以及接受 true 后抛原异常。四种最终均 `retained_selector=1 blurs=0`；程序化打开实际可见，未复活旧意图。内部组合不另算独立 CTest。

## 最终父验收

Windows MSVC HEADLESS，官方 preset、Ninja Multi-Config，最多 2 jobs；精确命令见 [driver.ps1](r3-artifacts/driver.ps1)。在原 76 项上增加两项 R3，去重组合包含全部 TreeSelect 18 项及原直接受影响代表。

```text
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r3-msvc.ps1 -Stage combined-build -Configuration <Debug|Release>
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r3-msvc.ps1 -Stage combined-test -Configuration <Debug|Release>
```

| 配置 | fresh build | CTest | CTest real | test wrapper |
| --- | --- | --- | --- | --- |
| Debug | exit 0，18.0759049 s | **78/78，exit 0** | 16.75 s | 16.8559082 s |
| Release | exit 0，25.3669009 s | **78/78，exit 0** | 9.26 s | 9.3907795 s |

每个配置实际编译 host／新测试并重新链接 36 个指定目标；不是仅运行旧程序。clang-format **22.1.3：803 文件、0 failures**；显式 OpenSpec **1.14.0** doctor healthy、strict **97/97**；diff checks 通过。

## 保留性与证据边界

Theme、shared Input／Focus／Pointer／Popover、RC、系统字体、Gallery 和五份金样均无本轮变更。保留 **41 个旧 IIFE、209 ordinal、17,612 typed leaves**；保护集合保持原 R2 的四处精确例外，其余 **129 文件**原文不变。

[index.json](r3-artifacts/index.json) 记录 28 份原始／派生文件、3 个源／CMake 指纹、72 个 PE、4 个 OBJ／2 个 LIB 和五份未变金样；72 个 PE32+ 均为默认 **1 MiB stack reserve**。原日志按字节保存；filtered Ninja 是派生摘录；不提交 EXE／OBJ／LIB，也不重生成金样。

本阶段属于 ROOT fresh 构建与 HEADLESS 运行；R3 独立审核仍是源码与已有程序证据。未运行完整 suite／Gallery／实际窗口或 GPU，不代表 Linux、系统 IME／AT 验收。
