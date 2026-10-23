# Windows 原生 Tabs 验收

2026-10-23，实际 Windows、MSVC x64/Ninja Multi-Config，`windows-msvc` Debug/Release；真实 `direct3d12` / `DXIL` 窗口。系统 display scale=1.5；每次日志记录400/600系统字体解析结果，包括 family、文件、face index 和 weight（400为 Segoe UI Variable Text，600文件为 SEGUISB.TTF）。

最终20次运行全部 exit=0：Debug/Release × Light/Dark × system/1/1.25/1.5/2。每次18张 GPU readback，共360张 PNG；两个JSON清单保存截图、UTF-8 LF规范化日志和EXE SHA256，并与最终两个本地EXE核对一致。

```powershell
python openspec/changes/060-20261023-add-native-tabs/evidence/run_native_matrix.py Debug
python openspec/changes/060-20261023-add-native-tabs/evidence/run_native_matrix.py Release
python openspec/changes/060-20261023-add-native-tabs/evidence/verify_native_evidence.py --check-executables
```

每次验证8个 Tabs：受控保留面板/Input/Select、Line small、Card middle、Card large bottom、centered bottom、8项溢出、响应label/disabled、局部主题。包含禁用导航、Home/End/Enter、父级拒绝/接受回写、自动composition/text提交、切换清理会话/嵌套弹层、草稿恢复、箭头/wheel与显露末项、width=160、真实窗口resize、3次idle polls、identity/content runs与dispose。

颜色局部性检查独立于响应标签/禁用变化，确认仅改颜色不重新测量该 Tabs。视觉抽查了 Light/Dark、2倍缩放的上下Line/Card和居中、焦点框/溢出末项、窄视口裁剪、局部主题和恢复后的编辑内容；所有PNG均经过解码、非空像素范围和哈希校验。

```text
windows-msvc Debug/Release complete builds: passed
affected native Tabs/Select/Card/Input/SDL/platform CTest: 18/18 each (5.34s / 3.90s)
Gallery Recording contracts: 8/8 each (72.83s / 16.44s)
native matrix: 20/20 runs, 360 PNGs, both EXE hashes passed
clang-format 22.1.3: 486 owned source files, 0 failures
openspec doctor: healthy; strict: 60/60; git diff --check: passed
```

平台通用完整110/110两配置见 `common-navigation.md`。本阶段输入为向真实窗口关联组件服务注入的逻辑事件，不是人工系统IME或完整OS事件来源验收。Linux native、Vulkan/SPIR-V与对应系统字体/输入没有实际证据，5.2保持未勾选；common和Windows保持完成，不自动archive。
