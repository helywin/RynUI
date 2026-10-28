# 原生组合与 Theme 阶段

基线：`d438e070c2815c5560dde836adab58b1b0d591ee`。本阶段完成 2.1–2.3 的实现和父任务实际验收；3.1 的独立 Astra 审查仍待完成，不能称整个 097 all_done。

## 实现范围

- 固定 String key 单值；真实 retained Input、Popover、Tree 与独立 host。无 renderer／OS SDK 依赖或组件私有 GPU 上传。
- 受控／默认值、open 和展开，title/disabled/visible 更新、未知值、重复值不清空、原生 clear、键盘与 selector 顺序的 Tab。
- 有限 popup／真实 Tree 滚动、三 Theme × 280/104 宽度；实际 GlyphInstance 裁剪与 R8 atlas、ordered fill、HitTest clip、首末行四边 outline coverage（各边 > 0.8），不只检查布尔 focus_visible。
- Form String/control(ref)、聚焦同值 reset、接受显示与完整 selector/editor/行身份；selector 固定只读，实际 focus 后不建立编辑 session，未伪称存在可编辑 preedit。
- 8 种回调卸载／私有结构退役／同 ref 替代代际／失活路径，5 种 Content 内局部 catch 挂载失败回滚，原异常和 fallback 保留。用户属性先在 owner Scope 解析为私有 Signal。
- 普通及嵌套弹层开合、祖先移动后一次同步定位；已关闭祖先恢复不复活子 popup。停稳后不请求帧／deadline、不重复发布 geometry。
- 独立 `tree_select.tokens.selector/tree`、继承、局部算法、最终校验、hash/equality/序列化、6 个追加 identity 与 dirty。selector 继续使用原生 Input 三尺寸字体；节点／空状态使用 Tree 字体。首版排除项见 API 文档，未新增 Gallery。

## 实际构建与运行

Windows MSVC 19.51、`windows-msvc-headless`，Ninja Multi-Config，最多 2 jobs。环境和全部精确命令见 [driver.ps1](host-artifacts/driver.ps1)。父任务实际新构建，不用其他 worker 的程序替代。

```text
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-msvc.ps1 -Stage combined-build -Configuration Debug
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-msvc.ps1 -Stage combined-test -Configuration Debug
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-msvc.ps1 -Stage combined-build -Configuration Release
powershell -NoProfile -ExecutionPolicy Bypass -File out/097-msvc.ps1 -Stage combined-test -Configuration Release
```

| 配置 | 最终组合 build | 最终 CTest | CTest real | test wrapper |
| --- | --- | --- | --- | --- |
| Debug | exit 0，20.3685461 s；此前受同一源码驱动的 88.6829436 s 部分目标构建记录另保留 | **72/72，exit 0** | 14.87 s | 20.1965086 s |
| Release | exit 0，120.8966699 s | **72/72，exit 0** | 7.20 s | 12.1568941 s |

test wrapper 包含本配置 fresh 金样导出、保留性脚本、CTest，不能当成纯 CTest 时间。此前 TreeSelect 自有集合 Debug/Release 各 **12/12**；最终 72 项包含这 12 项及新增 idle 断言，两个配置各项仅运行一次。

最终集合：TreeSelect 12、Tree 21、Popover 20、Form 4、AC R3 3、共享失败退役 1、Date reset 1、Time focus 1、portable Focus/Pointer/Input/Select/Theme/Modal 9。程序按 CMake 实际注册参数执行，CTest XML 验证 72 个不同 case 均无 failure/error。

## 保留性与产物

两个配置分别从新程序导出 default/dark/compact/dark-compact/compact-dark；五份跨配置字节一致并更新 checked-in golden。`verify-baseline.py --fresh <dir>` 对基线深比较保持 **17,612 个旧 typed leaves**（仅根 identity 允许改变），旧 **41 IIFE 原文／顺序**和 **209 ordinal 前缀**保持，追加 1 IIFE／6 identity。包括 RC／renderer、platform、输入／Focus、已审 Tree/AC/Date/Time/Popover/Statistic/Skeleton、Gallery 在内 **132 个文件**逐字节（仅 CRLF 规范化）保持。

[index.json](host-artifacts/index.json) 包含 33 份原始／派生产物记录、16 个源文件、66 个相关 EXE、8 个关键 OBJ、4 个 LIB 以及 5 个金样的 SHA256／大小／时间。66 个 PE32+ 均为默认 **1 MiB stack reserve**；未加 `/STACK`。filtered Ninja 是派生摘录，非完整日志。原始 `.log`／JSON/XML 的字节保留，不将诊断文件算为成功验收。

clang-format **22.1.3，800 文件、0 failures**；显式 OpenSpec **1.14.0** doctor healthy、strict **97/97**；diff checks 通过。

## 失败与诊断保留

1. 初次编译修正了 `.readOnly(true)` 的实参、GlyphInstanceStore 的 `.size()`、不存在的 seed.font_family 字段及测试中的 FormItem/Button API 用法；4 份原始错误日志保留。未改旧生产 API 来迎合测试。
2. 新嵌套初帧测试真实失败：`nested initial popup was not visible`。父 Popover 构建时临时阻断被提前解释为持久 suppression；修复为首次实际布局提交后判断祖先边界，本地 disabled/visible/readOnly/失活仍立即处理。新回归同时检查真正关闭的父层恢复不会复活子层。
3. 试验性 autoFocus 抛错探针错误地把首次布局回调当作 TreeSelect 调用栈内的异常；原始 `invalid-autofocus-probe.log` 保留，探针撤回。实际输入宿主在首次 geometry sync 才处理 autoFocus。挂载 getter／配置失败的 5 项回归与实际焦点回调销毁的 8 项回归继续保留，没有将该诊断描述为已修复的产品缺陷。
4. 首次扩大驱动从 CTest JSON 的 command 推导目标，忽略尚未构建程序会省略 command：部分 build exit 0，但 CTest 10 个程序 Not Run，另 1 个旧 golden 预期失败。修正显式目标集合并在测试前导出／深比较金样；最终完整 72/72 × 两配置。`initial-partial-build*` 与 `initial-missing-programs*` 保存，不冒称第一次扩大检查已通过。

## 验收边界

本阶段没有运行完整 CTest／Gallery frame／原生窗口或实际 GPU，也没有 Linux、系统 IME、AT 验收。逻辑 scene／CPU shader coverage、HEADLESS fixture 输入、实际 MSVC 构建与独立 source review 是不同证据；不能互相代验。Astra 复核与任务 3.1 仍待父任务继续处理。
