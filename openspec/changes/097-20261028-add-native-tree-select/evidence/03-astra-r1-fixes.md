# Astra R1 修复与父验收

基线：`b62519902638c275f43a4c5748bbd1f819571d46`。独立 R1 为源码推演发现两项 P2；本阶段先新增程序在该基线上实际复现，再修改生产。3.1 仍 unchecked，等待全新 Astra R2，不把本记录视作 CLEAN 结论。

## 两项复现与修复

### 受控外部关闭重试

`open=true`、初始无焦点，第一次外部空白点击由父级拒绝，第二次准备接受。原结果 `closes=1 visible=1`，第二次请求被内层 Popover 的 requested memo 去重，父无法接受。

根因属于 Manual 组合控制：接受 authority 没有变更，就不会触发同值 open reconcile。修复让 Manual Popover 在本次交付结束后把请求 memo 对齐当前接受 authority，下一次显式关闭可再请求；不改变 open、suppression、deadline 或绘制，不用 false→true 脉冲闪断弹层。普通 hover/focus/click 组合仍沿用原去重路径。

交付 guard 按原完整 ComponentId 重取存活状态；回调抛错时也完成 memo 收尾并保留原异常，回调销毁 owner 时不访问旧状态。模型方法只在 controlled 时处理请求 memo。

新增回归同时测试 TreeSelect 组合与直接 Manual Popover，普通／首次抛错共四种组合；每种做两轮拒绝／接受与显式重新打开，关闭后再次点击不多发请求，最后在关闭回调中销毁原 owner 并检查资源归零。

### 可聚焦键盘入口

原候选只查 shown/eligible/branch_active。Tree 禁用包装行仍 eligible 以支持独立富子控件，但自身 focusable=false，导致候选 A 被 FocusManager 拒绝，焦点留在 selector。

TreeSelect 候选补齐 focusable 条件，当前选中项不可操作时回退可聚焦行。新增五个场景：首行禁用、当前值为禁用首行、后行当前值禁用但首行可用、全部禁用后显式启用恢复、首行隐藏。检查实际 focused InteractionId 和 keyboard focus-visible，不改 Tree／FocusManager 原有公开语义。

## 实际执行

Windows MSVC HEADLESS，官方 preset，Ninja Multi-Config，最多 2 jobs。精确环境与命令见 [driver.ps1](r1-artifacts/driver.ps1)。

```text
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r1-msvc.ps1 -Stage build -Configuration <Debug|Release>
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r1-msvc.ps1 -Stage before -Configuration <Debug|Release>
```

修复前两个配置均 **0/2**，原始 CTest exit **8**。Debug real 0.16 s / wrapper 0.3018973 s；Release real 0.10 s / wrapper 0.242957 s。首个 outside 场景失败后未继续执行 direct Popover／扩展异常场景，不伪称这些也在基线上动态失败。外围 PowerShell 编排保留了最后非零 native exit，工具最终显示 exit 1；各 build/before 的实际结果独立记录于 JSON。

修复后曾跑初步 Debug **14/14**；随后扩展异常及回调卸载场景，执行最终去重组合：

```text
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r1-msvc.ps1 -Stage combined-build -Configuration <Debug|Release>
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r1-msvc.ps1 -Stage combined-test -Configuration <Debug|Release>
```

| 配置 | fresh build | 最终 CTest | CTest real | test wrapper |
| --- | --- | --- | --- | --- |
| Debug | exit 0，18.2567728 s | **74/74，exit 0** | 15.85 s | 15.9501587 s |
| Release | exit 0，27.8476447 s | **74/74，exit 0** | 7.67 s | 7.7808819 s |

最终集合是在原 72 项上追加两个 review case，包含全部 TreeSelect 14 项、20 个 Popover 以及原有 Tree／Form／AC／Focus／Input／Modal 等受影响代表。新场景属于 review case 内部组合，不冒称每个内部断言都是独立 CTest。

## 保留性与产物

- 生产只改 TreeSelect 键盘候选、Popover 交付 guard、PopoverModel 的纯 memo 收尾方法三个文件。
- Theme／ordinal／金样无修改或重生成；五份与 `b6251990` 逐字节保持。保留旧 **41 IIFE、209 ordinal、17,612 typed leaves**。
- `verify-baseline.py` 对旧保护集合仅放行两段精确的 Manual 请求追加，剩余 **131 个资源／输入／已审 host／Gallery 文件**保持原文，不使用忽略整个 Popover 文件的宽泛豁免。
- clang-format **22.1.3：801 文件、0 failures**；显式 OpenSpec **1.14.0** doctor healthy、strict **97/97**；diff checks 通过。
- [index.json](r1-artifacts/index.json) 记录 26 份原始／派生文件、5 个源／CMake 指纹、68 个 PE、6 个 OBJ／2 个 LIB 及五份未变金样的 hash／时间。68 个 PE32+ 均保持默认 **1 MiB stack reserve**。filtered Ninja 仅为派生摘录。

新增代码与程序对应的是本阶段 ROOT 新构建；独立 R1 的 24 次 existing-binary 复跑仍是其自己的历史证据。未运行完整 suite／Gallery／实际窗口或 GPU，也没有 Linux、系统 IME／AT 验收。
