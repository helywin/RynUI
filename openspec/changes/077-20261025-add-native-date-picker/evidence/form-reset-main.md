# 聚焦 Form reset：main 集成记录

## 来源与审查

2026-10-25，父线程从 `b28bd9523c6efd3782e363562911a0e883448158` 以 `git merge --ff-only` 接入独立修复 `0e0f11f7cb903f6b629ce02d5561621fbdb5cee4`。没有 rebase、冲突解决、TimePicker 源码或 Gallery 草稿导入；保留原有未跟踪截图。

全新独立 Astra xhigh 审查 `b28bd952..0e0f11f7` 的全部十个文件及周边 Form/Input/DatePicker 生命周期路径，结论 CLEAN、无可行动 P1/P2。审查实际运行 worker 已有二进制，Debug **24/24**、Release **24/24**；核对四十个 executable 的 SHA-256/时间戳及 MSVC、Ninja Multi-Config、HEADLESS/RECORDING 配置。该证据是已有二进制复验，不是独立重新构建。真实修复前失败及 worker Debug/Release 各 33/33、fresh golden 证据见 [form-reset.md](form-reset.md)。

## 父构建树定向验证

父树 `D:/code/RynUI` 的旧 headless 产物曾来自已中止的 TimePicker 临时合并，因此本次重新构建实际 main 源码，未将旧产物直接作为验收。内部 reset hook、DatePicker owner callback 与 worker 提交完全相同。

使用官方 `windows-msvc-headless-debug/release` presets、MSVC、Ninja Multi-Config，构建同一组 Input、DatePicker、Form、InputNumber、TextArea/Selection、Focus/session、Modal/Spin 与 Core/backend 边界目标。目标和 CTest filter 与 [worker 证据](form-reset.md) 的 `$portable` / `$components` 一致；父树仅将构建与 CTest 并行度设为 8，不改变用例。

```powershell
& ./out/date-form-reset-main.ps1 -Configuration Debug
& ./out/date-form-reset-main.ps1 -Configuration Release
```

本地 ignored helper 调用 `out/typography-dev.ps1`，最终执行官方 `cmake --build --preset windows-msvc-headless-<config> --target <上述目标>` 和 `ctest --preset windows-msvc-headless-<config> -R <上述 filter>`。原始日志位于 `out/date-form-reset-main-debug.log`、`out/date-form-reset-main-release.log`。

Debug 构建退出 0，CTest **33/33**，10.77 秒；Release 构建退出 0，CTest **33/33**，4.87 秒。两个配置均包含新增 `rynui.date_picker_review_form_reset`。

两个新构建的 Theme algorithm executable 分别执行 `--write-goldens`；每次生成后运行 `git diff --exit-code b28bd9523c6efd3782e363562911a0e883448158 -- design-tokens/ant-design/6.6.5/golden`，均退出 0，五份金样连 identity 在内全字节不变。原始输出位于 `out/date-reset-main-goldens-debug.log`、`out/date-reset-main-goldens-release.log`。

clang-format **22.1.3** 检查 **609** 个自有源码，0 failures；`openspec doctor --json` healthy，`openspec validate --all --strict --no-interactive` **80/80**，`git diff --check` 通过。父补充记录只有本证据文件，不改变已审查的实现或测试。提交后安全 fetch、推送 main 并比较 `git ls-remote origin refs/heads/main` 与本地 HEAD；远端 SHA 核对结果由本次父线程交付消息记录。

## 验收边界

本轮只验证 Windows MSVC headless 平台通用合同。079 worker 已收到经独立审查的修复 SHA，另行复验真实 Gallery 日期流程及同值 reset；本记录不勾选其 2.1/3.1/4.1。没有本轮 native GPU/窗口、Linux、系统 IME 或人工视觉验收声明。所有远端写入仅限 main，不推送 `t3/*`，不创建 PR 或 archive。
