# Windows 原生验收

2026-10-02，实际 Windows x64 / MSVC / Ninja Multi-Config。

- windows-msvc Debug/Release 完整 build；受影响 CTest 13/13，28.22 s / 7.29 s，另补运行 Button scene service 与 selection public API。
- 真实 SDL3 窗口、D3D12 / DXIL、默认系统字体链；实际 system display scale 1.25。
- Debug/Release × system/1/1.25/1.5/2 共十次运行，全部 exit 0；每次二十张 GPU readback，共 200 PNG。runs.json 保存参数、EXE SHA256、图像尺寸与 SHA256；validator 确认前后 EXE 没有变化、截图清单与视觉状态差异。
- 七个 Switch，十二个状态内容 slot，各挂载一次；Space 与 pointer 合计十一条 SDL 归一化输入。onChange/onClick 各两次、受控不回写候选一次；ref/autoFocus/focus/blur、Small 图标 RTL、loading/disabled 取消按压、窄宽 40/10/恢复、组件紫色主题、Default/Dark/Compact、resize 1420×900 与窗口失活均通过实际窗口中的断言。
- 有限 wave 开始/中段/结束保存 readback；静止/失活后 next_frame_deadline 为空，销毁后 ref、interaction、glyph scene 与 effects 全部释放。Gallery frame CTest 验证 on-demand idle 不重复提交；窗口夹具为状态 readback 显式提交四十帧，不把夹具的显式提交计作 idle。

人工查看 system initial 与 2× loading-cancel readback：中英文状态内容、Small 内嵌图标、紫色组件色、阴影、focus 与 loading spinner 均正常，完整样例处于窗口内。源码：examples/token_gallery/switch_acceptance.cpp。日志：out/046-windows-build-tests.log、out/046-windows-release-tests.log、out/046-windows-native.log。

Linux 原生验收独立 pending；此 Windows 证据不勾选 Linux tasks，也不要求重复已经验收的平台通用合同。
