# Windows 原生验收

2026-10-02，Windows/MSVC、Ninja Multi-Config，`windows-msvc-debug` / `windows-msvc-release`。

- 两个配置的完整 native build 成功。受影响 CTest：Debug 39/40 后修复本地链接缓存，单独 focus_state 1/1 通过；Release 40/40 通过（13.54 秒）。包含 Slider、Tooltip、window overlay、Theme、component mount/scene、Gallery、interaction registry 和 focus。
- `validate_native.py` 顺序运行 Debug/Release × system/1/1.25/1.5/2，共 10 个实际 SDL 窗口，D3D12/DXIL；系统 display scale=1.25。每次 21 张 GPU 回读图像，共 210 张 PNG。`runs.json` 保存命令、尺寸、退出码、当前 EXE 和图像 SHA256；脚本已逐项复核。
- SDL 注入后经真实 platform poll/normalization 的 keyboard/pointer/window events 验证整段拖动、动态插入、Delete/Backspace、拖出删除预览、失焦取消、逐端点禁用、SliderRef、纵向 reverse、受控回写、Default/Dark/Compact、resize。每次 root Content 执行一次。
- 人工查看 debug-scale-2 的 insert-captured/delete-preview，端点与文字、禁用状态、提示清晰。操作输入是自动化 SDL 注入，不作为物理键鼠或系统 IME 人工验收。

## 失败与修复记录

初次 debug-scale-1.5 在 Delete/focus 检查失败，独立重跑通过，保留 `initial-failed-debug-scale-1.5.log`。最终 fixture 给脚本事件设置时间戳并过滤真实桌面键鼠/焦点干扰；resize/render 生命周期消息保留。最终矩阵全部通过。

一次同时构建同一 native build 目录的 Debug/Release 引起 Ninja log 冲突。恢复为顺序构建后，focus_state EXE 在 main 前的 CRT `_initterm_e` 空地址调用崩溃，dumpbin 发现 DLL 导入表为空。仅删除该目标的本地 `.ilk` 并重新链接恢复导入表，测试通过；临时探针已移除，无焦点运行时代码修改。后续同目录只顺序构建配置。

补充修复：所有端点禁用时点击 marks label 不再发出 completion；headless Debug/Release Slider 合同与 native 受影响 CTest 均通过。

Linux Vulkan/SPIR-V/Fontconfig/Wayland 验收没有本机证据，task 7.1 保持未勾选。Tooltip 的细条 quad 箭头近似仍属于 040 已记录的视觉限制，后续共用图形收尾处理。
