# Drawer 平台通用实施证据

## 环境与范围

- 基线：`4e258e2afe832323c934e4ebfc3fb2144fc1608f`。
- 隔离 worktree：`C:/Users/jiang/.t3/worktrees/RynUI/t3-0288627d`，分支 `t3/native-drawer-component`。
- Windows MSVC 19.51，Ninja Multi-Config，configure preset `windows-msvc-headless`，build/test preset `windows-msvc-headless-debug`；编译最多4作业。
- 输出仅位于本 worktree 的 `out/build/windows-msvc-headless`；根 checkout 只读取并复制获准的 `out/typography-dev.ps1` helper，未修改根 checkout。

## 实际验证

在 Visual Studio Developer Environment 中执行等价正式命令：

```text
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 4 --target rynui_portable_drawer_model rynui_portable_drawer_public_api rynui_portable_drawer_theme rynui_portable_drawer_component rynui_portable_modal_component rynui_portable_modal_model rynui_portable_modal_public_api rynui_portable_modal_theme rynui_portable_modal_boundary rynui_portable_theme_algorithm rynui_portable_theme_runtime
ctest --preset windows-msvc-headless-debug --parallel 4 -R "^rynui.portable.(drawer_.*|modal_.*|theme_algorithm|theme_runtime|backend_boundaries)$"
```

最终聚焦 CTest **12/12 通过**，耗时2.52秒。包括4项 Drawer、5项原有 Modal、theme_algorithm、theme_runtime 与 backend_boundaries。没有重复运行全仓库 CTest，也未运行 Release 构建。

Drawer runtime 覆盖受控拒绝关闭、三种取消入口、closable/keyboard/mask 响应、透明遮罩隔离、Tab循环、组合输入 Escape 优先、首次隐藏与显示通知、关闭/再开保留草稿及 identity、会话过期、销毁与回调重入/退休、四边及动态 width/height、1×1窗口夹紧、固定 header/footer、正文滚轮/焦点显露、1.25字体scale像素相位、纯颜色不测量/不重跑slots、Modal→Drawer、Drawer→Modal、Drawer→Drawer、Select Escape、子TextArea滚轮与独立窗口clip。

1.25字体scale为真实字体栅格配置的 headless fixture；夹具构造会预缓存14px/1倍字体，本测试设置scale后清空自己的字体chain缓存，避免混用1倍正文与1.25倍标题。这不是原生显示器DPI或系统IME证据。

五份 Theme golden 已重新生成；逐份与基线 JSON 做语义比较，确认仅增加 `drawer` 和更新 snapshot `identity`，其余现有组件字段完全相同。Drawer theme 测试覆盖 Default/Dark/Compact、继承/reset、独立component seed/algorithm、五组dirty、无效覆盖原子拒绝及diagnostic identity。

clang-format **22.1.3** 的 `python scripts/format-code.py --check --clang-format <path>` 检查503个跟踪源文件，**0 failures**。OpenSpec doctor 健康，严格全量校验62/62通过，`git diff --check`通过。

## 父任务集成入口

- 公开入口为 `include/ryn/drawer.hpp`，由 `rynui.hpp` 导出，使用示例见 `docs/drawer.md`。
- 无需新增 window host：Drawer 使用现有 `WindowComponentServices::modal()`，其 `mounted()` 按逻辑挂载顺序包含两种面板，`snapshot(id).drawer` 区分 Drawer；`dialog` 是面板节点，body/footer/close沿用既有snapshot字段。
- 共享修改局限于 `modal_component.*` 的 mount/theme/measure/place/绘制分支，以及原有滚动metrics计算的小函数提取；栈、input boundary、focus restore、生命周期、子浮层和滚轮顺序仍用原实现。没有复制第二套Modal。
- Theme声明/runtime/snapshot、5份golden、CMake与umbrella header为必要共享文件修改；未改Gallery定义/计数测试/README/completion/support-overlay。

## 限制

首版无loading、动画、push位移、service、extra slot、resize/drag、动态销毁/懒加载、mask blur、RTL、平台可访问性桥接。阴影暂用可覆盖的全局elevated shadow，未映射四向专用shadow。没有原生Windows窗口/GPU/人工视觉、系统IME或Linux/Wayland验收；Gallery及真实平台证据由父任务集成。CPU scene v3/GPU ABI v2和renderer未修改。

## 提交与远端核对

实现提交：`e182b6a9f733d768843120add7987a077e455bb6`（`feat: implement retained native Drawer on shared modal runtime`）。提交后安全fetch；远端无同名分支时直接新建push，随后 `git ls-remote --heads origin refs/heads/t3/native-drawer-component` 返回相同SHA。此后仅补齐本证据与tasks完成标记，不改变实现或测试。

## 主分支集成和审查修复

Astra xhigh/priority 发现 Drawer rich title/footer 响应式变高时，正文视口改变却没有重新显露当前焦点。主分支集成比较新旧 body bounds 并设置 reveal；新增标题与页脚分别扩展三行的测试，检查末尾编辑框实际边界仍在正文内、滚动增加、焦点与身份保留。

与 Tag/Badge 同时解析 Theme 接线并重新生成五份 golden；语义比较确认除 drawer 与 identity 外，现有字段全部保持。实际 Windows MSVC headless Debug 聚焦15/15通过（3.58s），包含新 Drawer 回归、全部 Modal、Tag/Badge、Theme 与 backend boundaries。父任务同步 README 与常用组件清单。没有增加 Release 或原生窗口验收结论。
