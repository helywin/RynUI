# 聚焦 String Form reset 修复证据

## 基线与范围

2026-10-25，独立 `t3/native-date-picker` worktree 从已验证 `1ca5416` 安全 `git merge --ff-only b28bd9523c6efd3782e363562911a0e883448158`，无分歧且更新前干净。该基线包含 DatePicker/Form 和既有 Gallery；本次只修改 Input 内部 hook、DatePicker owner、定向测试及 077/API 文档，不修改 Gallery/root checkout，不导入 TimePicker WIP，不改变公开 API、FormValue 或 renderer ABI。

独立 Gallery worker 的只读复现见其 `079/evidence/stage2-date-reset-repro.md`：空 String authority，真实 TextCommitted `2026-10-`，保持焦点直接调用 reset，picker/editor 都仍是旧草稿。普通指针 reset 的失焦可掩盖问题，不能充当聚焦 reset 证据。本次 core 测试使用公开 Form/FormItem、真实 DatePickerRef、String ISO adapter、输入事件及直接 FormRef.reset，不通过内部 hook 驱动结果。

## 修复前实际失败

先添加 `rynui.date_picker_review_form_reset`，保留 production baseline 实现。新测试在未按 Enter 的日期草稿上验证普通 validate/help/Theme 不丢草稿或 preedit，然后保持真实 Input 焦点直接 reset。实际 MSVC Debug 命令：

```text
cmake --build --preset windows-msvc-headless-debug --target rynui_date_picker_review_tests --parallel 4
out/build/windows-msvc-headless/tests/Debug/rynui_date_picker_review_tests.exe form_reset
```

build 退出 0，测试退出 **1**，实际错误为：

```text
DatePicker same-value String Form reset retained draft before blur
```

根因：DatePicker 将 retained Input.value 绑定到包含草稿的 display；String 字段 reset 到同值不发通知，普通 Input reset 重读到的仍是 display 草稿，而非 DatePicker 已接受值。

## 实现与可观察回归

InputState 新增 `on_owner_form_reset`，仅内部 `InputComponentHost::configure_form_reset(ComponentId, std::function<void()>)` 注册。既有 reset 先取消 composition，重取状态，复制并调用 owner callback，再检查相同 component/editor 代际和 active scope，最后继续原有数字/字符串 reconcile。callback 自身或外部 authority getter 可以销毁控件，旧 state 不跨 callback 使用。DatePicker 挂载真实 Input 后注册：重读受控 authority（或保留非受控已接受值）、重取 owner/代际、清除 dirty draft、格式化 display、refresh；不递归 reset，不发 onChange/onOpenChange。普通同值 prop subscription 未改。已与 Time worker 对齐这一 hook 名称/签名，未导入其 focused getter 或其他源码。

新 case 覆盖：

- 空初值的 `2026-10-` 与合法但未提交的 `2026-10-20`；非空初值 `2026-08-07` 下输入 `2026-09-10`。全部通过真实 TextCommitted、无 Enter 和无 blur 建立草稿。最终另增完全无 preedit 的 `2026-10-` 直接 reset 序列，与 Gallery 原始失败操作一致。
- validate/help/status/Compact Theme 更新保留草稿和 composition；直接聚焦 reset 取消 preedit、清错误，恢复 picker snapshot 与真实 editor，保留焦点、组件数、DatePicker/Input/editor/popup/42 cells 身份。
- reset 后 Enter 不发布被重置的合法日期；Enter 的既有正常打开行为保持，不把它当作表单提交。
- 受控 field 初值不同于当前外部值，拒绝 reset 请求时保留当前外部 authority；接受时恢复初值。已打开日历在 reset 中保持打开，不发额外日期/开合请求。
- 销毁 DatePicker 后复用 ref，旧 FormItem reset 不重定向到新控件；整个 Form 销毁后 held refs 失效。受控 authority getter 在 reset 时销毁 owner/Input，复制中的 callback 安全结束，不继续访问销毁状态。

## 实际验证命令

在本 worktree 自有 `out/build/windows-msvc-headless`、Visual Studio 2026 x64 MSVC、Ninja Multi-Config 下运行。所有 build 最多 4 jobs；仅使用本树 binaries。以下 PowerShell 在 VS Developer 环境中分别取 `$configuration = 'Debug'` / `'Release'`，使用官方 presets：

```powershell
$portable = @('input_component','input_variant','input_actions','input_public_api',
    'input_display','input_caret_blink','theme_algorithm','theme_runtime','select_component',
    'modal_component','modal_boundary','focus_lifecycle','text_area','selection_component',
    'text_input_session','otp_component','otp_interaction')
$components = @('feedback_component','input_number_component','date_picker_model',
    'date_picker_public_api','date_picker_component','date_picker_theme','date_picker_review',
    'form_model','form_public_api','form_component','form_theme')
$targets = @($portable | ForEach-Object { "rynui_portable_$_" }) +
    @($components | ForEach-Object { "rynui_$($_)_tests" })
cmake --build --preset "windows-msvc-headless-$($configuration.ToLower())" --target $targets --parallel 4
& "out/build/windows-msvc-headless/tests/portable/$configuration/rynui_portable_theme_algorithm.exe" --write-goldens
git diff --exit-code b28bd9523c6efd3782e363562911a0e883448158 -- design-tokens/ant-design/6.6.5/golden
$pattern = '^rynui\.(portable\.(' + (($portable + @('backend_boundaries')) -join '|') +
    ')|' + (($components | Where-Object { $_ -ne 'date_picker_review' }) -join '|') +
    '|date_picker_review_.*)$'
ctest --preset "windows-msvc-headless-$($configuration.ToLower())" -R $pattern --output-on-failure
```

最终 Debug build/金样比较/CTest 均退出 0，**33/33**，32.03 秒；Release 同样全部退出 0，**33/33**，18.32 秒。包含全部 DatePicker R1/preflight、新 Form reset、Form 四项、InputNumber/TextArea/Selection、Focus/session、Modal、Spin 和 Core/backend 边界。最终增加完全无 preedit 的直接 reset 后已重新构建并通过两个配置的整组定向测试。

五份金样是本次新构建 executable 实际生成；Debug、Release 下均与 `b28bd952` 全字节相同（包括 identity），没有任何 Theme 源码改动。`clang-format 22.1.3` 检查 609 个自有源码，0 failures；doctor healthy，strict all **80/80**，`git diff --check` 通过。公开 headers、Theme、examples/Gallery、support overlay、completion 和 README 与基线保持相同；没有外部推送、PR 或 archive。

仅声明 Windows MSVC headless 的平台通用合同。未执行本次 Gallery、native GPU/窗口、Linux、系统 IME 或人工视觉；079 worker 等待本修复提交后独立复验，不能由本次测试勾选其任务。
