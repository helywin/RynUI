# Windows 原生验收

2026-10-02，Windows x64，MSVC / Ninja Multi-Config。

- `windows-msvc-debug` 完整 build 与受影响 CTest 12/12，22.56 s；Release build 与相同 CTest 12/12，6.53 s。
- 真实 Windows SDL3 窗口 / D3D12 / DXIL；默认系统字体链；系统显示缩放实际为 1.25。
- Debug/Release × system/1/1.25/1.5/2 共 10 次运行，每次 exit 0、17 张 GPU readback，总计 170 PNG。
- `runs.json` 保存 EXE SHA256、参数、图像尺寸与 SHA256；runner 校验每次运行前后 EXE 不变、图片清单、全部 PNG 和当前 EXE hash。

原生窗口中挂载八个 Divider，验证水平/垂直 Dotted、Dashed、Solid，1/4 logical pixel 线宽、透明间隙，Small/Middle/Large，LTR Start 与 RTL Start/End，长度零/20 dp，Default/Dark/Compact，resize 1420×900 后 rail 宽度响应，content/title slot 各只运行一次，interaction 始终为零且 idle 无动画 deadline。销毁后全部 effects 释放。

人工查看 system thick-dotted 与 2× Large 图像：圆点形状和间隙正常、水平/垂直线条与标题对齐，完整 Large 内容在窗口内。源码夹具为 `examples/token_gallery/divider_acceptance.cpp`。日志：`out/045-windows-build-tests.log`、`out/045-windows-native.log`。

Linux GCC/Clang、Vulkan/SPIR-V、Fontconfig/Wayland 原生 checkbox 保持独立 pending；Windows 结果不替代 Linux 证据。共同逻辑已经在本机 headless 验收，无需为同一合同要求 Linux 重复运行。
