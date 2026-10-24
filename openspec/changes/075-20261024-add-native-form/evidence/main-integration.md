# 父线程 Form 集成（2026-10-25）

以 main `fcca5e8426881da6980c2a46f7f0deba2d069139` 为基线，显式 `git merge --no-ff --no-commit 275707a76d6149fd0b03071b0293873eb04f7581`，自动合并无冲突。保留 worker 的注册/控件/reset/真实 ref 焦点与字体修复，以及主分支 076 的真实目录跳转、073 Theme 子树预校验及已有 popup wheel/host 顺序。没有重新播放或压平 worker 历史。

## 关闭算法时的局部 seed

父线程补充实际回归：父 Theme 覆盖 Form label 与 gap，子 Theme 的 `form.algorithm=false` 但 `form.seed.font_size=18`。修复前 `rynui.form_theme` **0/1**，明确失败 `inactive Form seed discarded inherited parent overrides`；ignored `out/form-inactive-seed-before-fix.log`。现将关闭的局部 seed 从 Form 继承判定移除，不改动 26 个既有组件引用 IIFE。

新增静态及响应回归覆盖：父级 token 继承、子显式 override 叠加、inherit=false、未启用 seed 更新不替换 snapshot/不通知、父更新正常传播、开启算法应用 seed、关闭恢复继承。新增测试编写期间纠正了 ThemeScope 工厂参数及 snapshot 返回引用的使用；最终实际编译运行通过，不把中间编译失败计为产品验收。

## 实际验证

Windows MSVC、Ninja Multi-Config、`windows-msvc-headless`：原始合并版本 Debug **20/20，14.82 秒**；补充继承修复后的最终 Debug **20/20，13.88 秒**、Release **20/20，14.83 秒**。集合与 [worker 合并验收](astra-r1.md#合并父线程已验证基线) 的 20 项相同，包含 Form model/public API/component/Theme、InputNumber、Feedback、Empty/Result 三项、Table 和 portable Theme/Selection/Modal/focus/TextArea/Input/Dropdown/Popconfirm/Select。

父线程使用 `out/typography-dev.ps1 -Preset windows-msvc-headless`，对该 20 个 targets 构建后以 `^rynui\.(form_.*|input_number_component|empty_result_.*|table_component|feedback_component|portable\.(input_component|selection_component|select_component|text_area|theme_runtime|theme_algorithm|modal_component|focus_lifecycle|dropdown_component|popconfirm_component))$` 执行 CTest；Release 显式加 `-Configuration Release`。ignored 日志为 `out/form-main-{debug,debug-final,release}.log`。

执行本次新构建的 Debug Theme algorithm `--write-goldens`，再用 worker 的 `out/verify-form-goldens.py fcca5e8426881da6980c2a46f7f0deba2d069139`，五份旧数据除 identity/新增 Form keys 外逐项保留，26 个旧引用 IIFE 原文/顺序不变。五份 fresh golden 相对 worker `275707a` 字节无差异。clang-format 22.1.3 检查 **596** 文件，零失败。

Form 目录从 planned 更新为有证据的 partial，明确首版和未覆盖范围；重新生成离线 catalog，文档提供真实 API 入口。fresh `windows-msvc` Debug Gallery/frame/catalog 构建通过，实际 `--inventory-only` 保持 **273 IDs、93 navigation、308 Buttons、906 interactions、583 visible、279 live、135 Inputs**；八项 Gallery contracts **8/8，0.49 秒**。`--smoke` 实际 **D3D12/DXIL、display scale 1.5、7 frames、exit 0**。未重复行为未变的完整 Gallery frame；这里的 smoke 仅为新 Theme/host 组合后的窗口启动/自动主题切换/退出证据，不是 Form 人工视觉或系统 IME 验收。ignored 日志 `out/form-main-{gallery-build,catalog-build,inventory,gallery-contracts,native-smoke}.log`。doctor healthy、strict **78/78**、working/staged diff check 通过。

## 独立审查与边界

Astra R2 固定 worker `275707a` 审阅原始完整 075 范围与全部 R1 修复，**未发现可操作 P1/P2**；独立运行 worker 既有 Debug **20/20**、Release **20/20**。注册代际、authority/reset/重入/销毁、真实 ref 目标和字体校验均确认符合首版合同。父线程新增的 seed 继承差异不在其固定提交内，将独立复核后记录，不提前声称已审查。

没有运行全套 CTest，不将 headless 结果作为 GPU、Linux、系统 IME 或人工视觉验收。父线程只推送 main，禁止推送 worker/t3 分支；不创建 PR 或 archive。
