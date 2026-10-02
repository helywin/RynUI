# Windows Flex 原生验收

2026-10-03，Windows/MSVC/Ninja Multi-Config，`windows-msvc` Debug/Release。受影响原生 CTest 各 10/10（28.26s/9.51s），包括 Flex features、Input/GPU、Button、选择控件、Text、Typography 与 Gallery frame。D/R 默认栈 `rynui_token_gallery.exe --smoke` 均通过，日志 native-debug.log、native-release.log。

`flex_acceptance.cpp` 通过实际 SDL 窗口、DefaultFontChain 系统字体、D3D12/DXIL backend 与 SceneResources 共同上传路径验收。系统 display scale 1.25；Debug/Release 系统及强制 render scale 1/1.25/1.5/2 共 10 次，13 个状态/次，共 130 张 GPU 回读 PNG。不是软件截图或纯 headless 结果。初始混合基线及 Release 2x 纵向反向 RTL 图已人工查看。

每次运行检查真实 Text、Typography、Button 标签、Input snapshot、嵌套 70 dp Stretch Button 的文字基线相等。验证 wrap、wrap-reverse、H RTL、物理 Left/Right、V RTL 与 cross reversal XOR；方向更新保留 measure/shape 次数。SDL 注入 3 次规范化指针事件，更新后的命中实际触发一次 click。字体 28→36 dp、Dark、1420x900 resize 后重新检查基线。content_runs=1、label_runs=5、26 次提交；inactive 后 3 次 idle poll 无 frame request/deadline/额外提交；销毁 ref、nodes、interaction、text/effects 全归零。

复现与证据校验（需要 Pillow）：

```powershell
python openspec/changes/049-20261003-complete-native-flex-layout/evidence/windows/validate_native.py
python openspec/changes/049-20261003-complete-native-flex-layout/evidence/windows/validate_native.py --verify-only
```

脚本启动真实程序，检查 EXE SHA256、退出码、日志合同、13 张/次 PNG 尺寸、不同视觉状态的差异，并保存 runs.json 的 PNG hashes。后续重建 EXE 后历史 hashes 不再对应当前 binary；历史记录保留当时实际运行身份，不作为未来版本验收。平台通用 65/65 另见 ../common；Linux 真实字体/GPU/window/input/DPI 仍独立待验收。
