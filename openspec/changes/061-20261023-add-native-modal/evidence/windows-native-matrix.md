# Windows Modal 原生窗口验收

2026-10-23，Windows x64 / MSVC，`windows-msvc` preset，Ninja Multi-Config。实际源码提交为 `df43421264731bae5c43939fdeaaddbcc3b9e5fb`；本记录晚于该提交补入，后续反馈组件集成不计入本次原生二进制证据。

## 构建与平台测试

Debug 与 Release 均完成全部默认 targets 构建。平台/input/event pump/lifecycle/SDL text input/default font chain 以及 Modal/Drawer/Popconfirm/TagBadge component 的相关 CTest 各 9/9 通过，Debug 3.36s、Release 1.32s。完整输出见 [Debug](windows-native/full-debug.log) 与 [Release](windows-native/full-release.log)。这些平台测试不替代其他 change 的真实窗口验收。

## 实际窗口矩阵

对 Debug/Release、Light/Dark、system/1/1.25/1.5/2 的全部组合运行 `rynui_token_gallery --modal-acceptance`，20/20 成功。每组 22 张捕获，共 440 张 PNG；全部核对尺寸与 SHA256，完整清单、日志正文、原日志 SHA256 和二进制 SHA256 见 [manifest](windows-native/manifest.json)。原日志保存在同目录的 20 个 `<configuration>-<theme>-<scale>.log`；全部 PNG 保留在本机 `out/modal-native-matrix/`，仓库保留四张抽查图。

- 实际 GPU driver `direct3d12`，shader format `DXIL`，system display scale 1.5；显式 render scale 分别为 1/1.25/1.5/2，system 使用 1.5。
- 字体实际加载 Segoe UI Variable Text (`C:\WINDOWS\FONTS\SEGUIVAR.TTF`, weight 400) 与 Segoe UI (`C:\WINDOWS\FONTS\SEGUISB.TTF`, weight 600)，face index 均为 0。
- 每组验证模态边界、Tab/反向 Tab 与背景引用拒绝、受控关闭拒绝、取消/确认/loading、自动 composition/commit、保留草稿、Select/Tooltip Escape 优先、嵌套焦点恢复、正文滚轮/末尾输入显露、resize、居中/无遮罩、idle 和 dispose；全部 exit_code=0、disposed=1。
- Debug EXE SHA256：`b295db8ace1427aca3a5ec2f6795d1a3a11475490a74e3594e2ee3db24df8642`。
- Release EXE SHA256：`17e1b8aab2c3342e816ebcba982bb6a227279e1edb995ff77d610eeaab19c4c4`。

抽查图：[Dark/2 打开](windows-native/release-dark-2-opened.png)、[Light/1 正文末尾](windows-native/debug-light-1-body-end.png)、[Dark/system 嵌套](windows-native/release-dark-system-nested.png)、[Light/1.25 resize](windows-native/debug-light-1.25-resized.png)。标题、固定页脚、末尾输入显露和嵌套遮罩符合对应场景；备注中的 emoji 显示缺字，字体覆盖不计作通过。

## 复现入口与边界

在同一源码提交的 Visual Studio Developer Environment 中使用 `cmake --preset windows-msvc`，分别通过 `windows-msvc-debug`、`windows-msvc-release` build preset 构建全部 targets。每个配置执行以下入口，替换明暗主题和缩放组合；system 组合省略 scale 参数：

```text
out/build/windows-msvc/examples/Debug/rynui_token_gallery.exe --modal-acceptance --evidence-dir=out/modal-check --acceptance-theme=dark --acceptance-scale=1.25
```

这是实际原生 D3D12 窗口中的自动交互和抽样图像核对，没有声称人工系统 IME、全量人工视觉或 Linux/Vulkan 验收。Linux 6.2 保持 pending。
