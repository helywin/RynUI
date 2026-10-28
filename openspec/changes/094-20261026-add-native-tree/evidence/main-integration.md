# 已审 Tree / AutoComplete 与主分支组合验收

## 源码身份与范围

父 main：`48bb20f8bfb9663cc48adeaae58cbc946aec7af1`，已发布 Avatar、Rate、Statistic 和 095 原生 RC。
显式 merge 第二父：`972ca5bcaaa34ac90207da7e4703dd0ec032d360`，包含 Tree Astra R3 CLEAN 与
AutoComplete `ad779c59` / Astra R4 CLEAN 的完整历史。

此记录属于父工作区的新构建，不把 worker 的已有二进制结果冒充 main 验收。
两组件保持各自声明的首版边界，尚无交互 Gallery、原生 GPU、Linux、系统 IME 或 AT 验收。
未导入 093 Gallery；未改 renderer ABI、HLSL、RC 资源接入或系统字体默认行为。

## 合并与保留性

Theme 冲突按已发布 main 顺序处理，再追加 Tree / AutoComplete。源码检查确认：

- **39 + 2 = 41 个解析 IIFE**，原文与 main 顺序保持，worker 新块原文保持。
- **202 + 7 = 209 个 TokenIdentity**，已发布 ordinal 前缀保持。
- 新 Tree 的未发布 worker ordinal 198/199 调整为 main 后的 202/203；保留绝对断言并增加相对 Statistic 的追加断言。
- **25 个 main 资源/组件文件、7 个已审 worker host/共享文件**与指定父原文一致。
- 两个独立组件测试程序各导出五份真实组合快照；Debug、Release、Tree exporter 和 AutoComplete exporter 均逐字节一致。
- 保留 main **16,882**、worker **17,457** 个旧标量值及类型，仅新增组件字段与 snapshot identity。

检查脚本为 `verify-main-integration.py`，验证两套父数据和所有待写入金样后才更新 tracked 文件。
Statistic 除下述两个失败边界外的源码原文保持；其 R1–R7 断言保持，新断言更强。

## Statistic 共享失败退役兼容修复

组合检查发现：Statistic 的初始 formatter/富 slot 抛出时，普通销毁仍会交付聚焦 Input 的 blur。
若该 blur 也抛出，资源释放不能完成。原 R6 的 11 个用例没有危险 blur 处理器。

在生产修改前追加 `FormatterBlurThrow` / `SlotBlurThrow`：新两项真实失败，原 11 项通过，原日志
`debug-statistic-before.log` 保存局部 catch 和失败编号 11/12。
修复只复用已有 `retire_failed_component(full ComponentId)`：

1. 外层挂载回滚改用同一静默退役入口。
2. rich region 的用户 callable 内使用同一回滚 guard，先于 `mount_slot` 普通子树回滚清空事件源。

没有第二套 cleanup 模式或新公开 API；正常组件销毁/blur 保持原行为。
原异常、旧资源归零及替代 generation/ref 保持由现有回归和新增零 blur 断言覆盖。
修复后原 11 + 新 2 用例通过，并纳入以下组合集合。

## 实际新构建与测试

Windows MSVC、正式 `windows-msvc-headless` configure、Ninja Multi-Config；build/CTest 最多 2 jobs。
完整目标及精确过滤器随 `main-integration-artifacts/commands.ps1` 保存。

```powershell
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target <记录中的45个目标>
cmake --build --preset windows-msvc-headless-release --parallel 2 --target <同一目标集合>
ctest --preset windows-msvc-headless-debug --parallel 2 --output-on-failure -R <记录中的精确过滤器>
ctest --preset windows-msvc-headless-release --parallel 2 --output-on-failure -R <同一过滤器>
```

实际去重集合 **Debug 98/98，Release 98/98**；CTest real **25.34 s / 11.89 s**。
包含 Tree 全部 model/Theme/component/review、AutoComplete 原首版/R1/R2/R3、共享失败退役、
Focus/Pointer/Input/Select/Modal/Layout、Popover、Date/Time/Form，以及 main Avatar/Rate/Statistic/Skeleton 和 Theme 代表。
90 个实际项目 PE 均保持默认 1 MiB 栈；文档测试使用的系统 Python 单独记录，不冒充项目产物。
独立产物与 CTest inventory、JUnit、PE 信息、源码 SHA256、原始 build/test 日志随 artifacts 索引。
这些结果不扩大为未选择的 full suite 或平台专属验收。

构建过程保留两项诊断：最初目标清单误含不存在的 Avatar model/API 独立目标，修正为实际两个目标；
第一次全构建命中 Tree worker 的旧 ordinal 断言，按已发布 main 追加位置修正后重新构建通过。
原有编译 warnings 保留，未为无关 warning 修改既有组件。

## 独立审查

全新 Astra xhigh / default 组合 R1：**CLEAN**。仅做源码与双亲 diff 审查，不运行程序。
核查 Theme 全链、共享 Focus/取消/Select/Popover 定位、host 顺序、两处 Statistic 失败边界的 callable/guard 生命周期及代际隔离。
审查绑定上述 dirty merge；生产源码在审查至验收期间未漂移，Statistic SHA256 为
`e8267f8c042bd29e0e6127693e775160979bac205aaad4667e6cb8460849c344`。
父新构建证据与 source-only 审查分别记录。

通过阶段提交推送只使用 main，核对远端 SHA；不推 worker 分支，不创建 PR 或 archive。

最终检查：clang-format **22.1.3，792 文件、0 failures**；显式 OpenSpec **1.14.0** doctor healthy、
strict **95/95**；staged/unstaged diff 检查通过。raw artifacts 使用局部 Git 属性保留原字节，
SHA256 索引同时核对工作树文件与最终 staged blob。
