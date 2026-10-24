# DatePicker 父线程集成（2026-10-25）

在 main `6f3e794092d183f7b53c21bbf3a481f095431b5d` 上执行 `git merge --no-ff --no-commit 1ca5416dda51f301f06e7693d473350a34ee4cc6`，自动合并无冲突。worker 已包含 Form/main `7ff4deb`；保留父主分支之后的 Form 审查收口和 079 规划。父线程没有重写 DatePicker/Form 生产实现；`git diff --quiet 1ca5416 -- src include tests` 退出 0。

## 共同逻辑

本机 Windows、MSVC、Ninja Multi-Config、`windows-msvc-headless`，使用 ignored `out/date-picker-main.ps1` 构建 [r1.md 最终 39 项集合](r1.md#最终联合验收)，再执行相同 filter 的 CTest。该包装读取 `r1.md`/`common.md` 的 portable/targets 集合，调用既有 `out/typography-dev.ps1` 初始化 MSVC 并使用正式 preset；具体调用为：

```powershell
.\out\date-picker-main.ps1
.\out\date-picker-main.ps1 -Configuration Release
```

父线程实际 **Debug 39/39，7.83 秒；Release 39/39，5.01 秒**，包括 DatePicker 四项、四个 R1/继承 preflight 回归、Form 四项、Input/Number、Button、Theme、Table、Feedback/Spin、popup/Modal/focus/pointer/Recording 与 Core/renderer negative configure guards。不是重复全量 CTest 或跨平台矩阵。ignored 日志 `out/date-picker-main-{debug,release}.log`。

新构建 Debug Theme executable 运行 `--write-goldens` 后，五份结果与 worker `1ca5416` 字节相等。通过 ignored `out/check-date-picker-integration.py` 比较：相对 main `6f3e794` 只新增 datePicker 与 identity，旧数据值/类型及 **27 个既有引用 IIFE（含 Form）** 保留；相对原 DatePicker `4ee44ad` 只新增 form 与 identity，26 个其他旧 IIFE 保留，DatePicker 自身 IIFE 仅包含已验证的 R1 inactive seed 继承修复。最初比较脚本错误地要求修复前的 DatePicker IIFE 原文不变，随后明确排除此已知修复块再通过；未改变生产实现来迎合比较。

## 目录与 Windows 启动

DatePicker 从 planned 更新为 partial，声明 ISO 单日期/真实月历/限制/footer/Theme 的已实现范围及 Range/locale/格式/动画/Web/平台缺口。重新生成 catalog，更新 API 入口及完成清单，尚未声称 079 的实际 DatePicker 样例已完成。

`windows-msvc` Debug fresh Gallery/frame/catalog targets 构建通过，`--inventory-only` 保持 **273 IDs、93 navigation、308 Buttons、906 interactions、583 visible、279 live、135 Inputs**；八项 Gallery contracts **8/8，0.54 秒**。新二进制 `--smoke` 实际 **D3D12/DXIL、display scale 1.5、7 frames、exit 0**。ignored 日志 `out/date-picker-main-{gallery-build,inventory,gallery-contracts,native-smoke}.log`。这只验证组合 Theme/host 之后的 Windows Gallery 启动/自动主题切换/退出，不代表 DatePicker 弹层的原生 GPU/人工视觉、系统 IME、Linux 或辅助技术验收；没有重复既有完整 Gallery frame。

## 独立审查

Astra R2 固定 worker `1ca5416dda51f301f06e7693d473350a34ee4cc6` 复核完整 077 范围、三项 R1 修复和 Form 合并，**CLEAN，无可操作 P1/P2**，声明首版在其 headless 边界内完整。实际独立运行 worker 既有 DatePicker 四项/review 四项、Theme algorithm/runtime、Form 四项：Debug **14/14**、Release **14/14**。源码核对极端公历、ISO/IME/authority/suppression、保留身份、Modal/Spin/footer/popup/裁剪/销毁/空闲，以及 root 默认配置栈修复、堆上预校验、binding bridge、自引用 guard、26 个原组件 IIFE 和双基线 golden。无编辑、构建或新平台声明。

clang-format 22.1.3 检查 **609** 源文件零失败，doctor healthy、strict **79/79**、working/staged diff check 通过。父线程只推送 main 并核对远端 SHA，不推送 t3 分支、不创建 PR 或 archive；提交与核验完成后更新 task 4.1。

最终集成提交 `0fdefbae9d5fb67422d6a67471019d5e3227dac1` 已推送 main，远端 `refs/heads/main` 与本地 SHA 相同；远端 `t3/*` 查询为空。按实际完成的提交/推送与前述验证，task 4.1 完成，077 总计 **4/4**。
