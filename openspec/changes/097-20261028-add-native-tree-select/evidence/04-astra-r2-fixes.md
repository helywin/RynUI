# Astra R2 修复与父验收

基线：`9d719df661c43807897563b72b8858fedcec1756`。独立 R2 的两项 P2 为源码推演；本阶段在该生产基线上新增程序实际复现，再修改生产。3.1 保持 unchecked，等待全新 Astra R3。

## 同一次外部点击的关闭交付

原组合中，指针 down 先使 selector／Tree 行失焦，随后 Popover outside observer 再请求关闭。受控父级拒绝第一次、准备接受第二次时，一次点击就出现 `closes=2 visible=0`，越过了下一次操作才重试的约定。

PointerRouter 现在只在一次原生 dispatch 及其回调期间提供内部 token，正常返回和异常出口都清空；合成取消不创建新的 token。TreeSelect 将关闭回调集中交付，同一次 token 的焦点退出与 outside 意图只交付一次，下一次原生事件可再次请求。状态按原完整 owner generation 持有，不使用诊断计数作为业务状态，也不改变原生 pointer／focus 回调。

无 token 的程序化焦点退出仍同步交付，不依赖下一帧采样。Tab 改由 settled 焦点退出交付关闭，避免按键处理与 deferred 焦点观察分别通知。回调中强制 suppression 仍执行实际封锁，只合并重入的关闭通知；关闭请求同时清除待进入树的键盘焦点意图。

最终 `focused_outside` 在 selector／Tree 行焦点、普通／首次抛错共四种组合中验证：首次拒绝保持打开、下一次点击接受、不同点击 token 不同、异常后 token 清除且原异常保留；继续检查受控 Tab 到相邻 Input 只通知一次、后续点击仍能接受，以及两次独立程序化焦点退出各自立即通知。原 R1 回归未修改。

## Manual 收尾尊重 suppression

直接 Manual Popover 的关闭回调只设置 disabled=true 时，回调中的 suppression 已完成请求 memo；原 guard 又用仍为 true 的受控 authority 覆盖 memo，同一指针的后续观察再次通知 false。

`settle_request()` 仅在 controlled 且未 suppressed 时对齐 memo。已强制关闭时保留 suppression 的请求状态，不修改接受的 open、不主动解除 suppression。新增回归同时覆盖直接 Manual Popover 与 TreeSelect：第一次仅一次通知；后续观察不重复；解除 disabled 不复活；显式 authority false→true 后可重新打开并再次恰好通知一次。

## 实际执行

Windows MSVC HEADLESS，官方 preset、Ninja Multi-Config，最多 2 jobs；精确环境与命令见 [driver.ps1](r2-artifacts/driver.ps1)。

```text
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r2-msvc.ps1 -Stage build -Configuration <Debug|Release>
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r2-msvc.ps1 -Stage before -Configuration <Debug|Release>
```

修复前两个配置均 **0/2，CTest exit 8**。Debug real 0.16 s／wrapper 0.2973949 s，Release real 0.10 s／wrapper 0.2393823 s；原始输出分别为 `focused_outside tree=0 first_closes=2 visible=0` 和 `suppressed_close compound=0 closes=2 visible=0`。每个 case 在首个组合即失败，后续组合及后来追加的异常／Tab／程序化场景不冒称在基线上动态失败。外层驱动检查预期 exit 8 后正常结束。

初步 Debug 的 16/16 通过后扩展上述事件与异常组合，再执行最终去重集合：

```text
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r2-msvc.ps1 -Stage combined-build -Configuration <Debug|Release>
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-r2-msvc.ps1 -Stage combined-test -Configuration <Debug|Release>
```

| 配置 | fresh build | 最终 CTest | CTest real | test wrapper |
| --- | --- | --- | --- | --- |
| Debug | exit 0，225.310167 s | **76/76，exit 0** | 16.00 s | 16.1039977 s |
| Release | exit 0，244.6908914 s | **76/76，exit 0** | 7.64 s | 7.7289279 s |

集合包含 TreeSelect 全部 16 项、Tree 21 项、Popover 20 项，以及 Form、AC R3、静默退役、Date／Time、Focus／Pointer／Input／Theme／Modal 代表。case 内部组合不另算独立 CTest。所有指定目标在本次构建中重新链接；初步只重建新目标的运行不代替最终组合验收。

## 保留性与边界

- 生产仅修改 TreeSelect host 的关闭交付、PopoverModel 一项 memo 条件，以及 PointerRouter 内部 dispatch token；没有修改 FocusManager、renderer、RC、系统字体或 Gallery。
- Theme／ordinal／金样未修改或重生成，五份与基线逐字节保持；旧 **41 IIFE、209 ordinal、17,612 typed leaves** 保持。
- 保留性脚本对旧保护集合仅放行两段精确 Manual 请求追加和两个 PointerRouter 文件内的精确 token 追加；其他 **129 个资源／输入／已审 host／Gallery 文件**保持原文。
- clang-format **22.1.3：802 文件、0 failures**；显式 OpenSpec **1.14.0** doctor healthy、strict **97/97**；diff checks 通过。首次格式检查因当前 PATH 无 clang-format 未启动，随后使用明确的 22.1.3 入口完成全量检查。
- [index.json](r2-artifacts/index.json) 记录 22 份原始／派生文件、7 个源／CMake 指纹、70 个 PE、6 个 OBJ／4 个 LIB 及五份未变金样的 hash／时间；70 个 PE32+ 均保持默认 **1 MiB stack reserve**。不提交 EXE／OBJ／LIB。Ninja 摘录是派生记录，修复前 CTest 与最终 build／CTest 日志按字节保留。

本记录属于 ROOT 本阶段新构建与 HEADLESS 运行，独立 R2 的 existing-binary 复跑与 source-only findings 保持各自边界。未运行完整 suite／Gallery／实际窗口或 GPU，未验收 Linux、系统 IME／AT。
