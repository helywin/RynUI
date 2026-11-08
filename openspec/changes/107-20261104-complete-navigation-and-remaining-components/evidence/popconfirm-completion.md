# Popconfirm 与 App 服务收口

2026-11-08。原始日志、RGBA 回读和局部截图仅保留在忽略的 `out/resume-20261107/popconfirm-completion/`。

- Windows MSVC `windows-msvc-headless`：Debug、Release 各 **37/37**。覆盖六组新 Popconfirm 行为、旧 Popconfirm 合同、Provider 来源、嵌套 Modal/i18n、App 服务及受影响 Modal motion。修复输入域重入导致的焦点转移/恢复和挂载时序问题；悬停不抢焦点，静态点击不保留触发计时，空说明不保留空白行，动态显隐保持身份；服务关闭后拒绝更新。Debug Theme 夹具入口栈溢出经 CDB 确认，去掉 reject 参数的完整 ThemeConfig 值中转后通过，保留全部非法值与发布原子性断言。
- Windows MSVC `windows-msvc`：Debug、Release 各 **3/3**，实际 hidden D3D12/DXIL 回读，包含 source closure/反射布局。新增 Popconfirm 四向箭头、50%/85% alpha、1/1.25× 的联合接缝检查；固定两字节容差，验证一次混色及完整片元覆盖。正常关闭恢复锚点焦点，清除该预期焦点后整帧与关闭前基线相等。
- Windows Release Gallery：Modal、Popconfirm、App 各四组配置，分别 **33、28、136** 个捕获，退出码和布局告警均为 0。覆盖三种新增触发入口、中间动画帧、富 Input、自定义按钮和真实 App panel 服务；定点局部图确认 Input 四边完整，示例标题与双按钮一致。
- Windows HEADLESS Debug Gallery 合同 **3/3**：`gallery_page_modal_frame`、`i18n_gallery`、`gallery_frame_contract`，保留稳定身份清单和双语编辑行为。
- Linux `linux-gcc-release`：**3/3**，独立 Mutter 46.2 Wayland、Vulkan/SPIR-V；实际设备为 Mesa 25.2.8 llvmpipe（CPU 软件驱动），不是物理 GPU 验收。相同回读覆盖新增 Modal motion/geometry 与 Popconfirm 接缝。Modal、Popconfirm、App 各四组配置，分别 **33、28、136** 个捕获，退出码和布局告警均为 0。

触发器支持 Click/Hover/Focus/ContextMenu/Manual 和组合；原生触发控件禁用/loading 不允许 wrapper 绕过。真实 ButtonProps 的用户回调执行后重新核对当前资格。局部截图发现富 Input 被消息区裁断，新几何断言先复现失败；消息区为图标保留宽度后收缩，LTR/RTL 均检查 Input 完整落在内边距中，保留编辑会话并镜像页脚。有限退出立即撤输入，终帧后先完成服务 future，再调用完成通知。窗口失活、reduced、隐藏和退役停止持续调度。

系统 IME、跨实际显示器和 OS accessibility adapter 不由这些自动检查推导。Progress 的 paint/cap 数值认证及全组件分平台汇总仍独立跟踪。

关键命令（Release 使用同组 target/正则；原生目标为 `rynui_composition_native_readback_tests` 与 `rynui_token_gallery`）：

```text
ctest --preset windows-msvc-headless-debug --output-on-failure -R "^rynui\.(popconfirm_completion_|popconfirm_component|app_panel_|modal_completion_|portable\.popconfirm_|i18n_components_overrides|config_provider_)"
ctest --preset windows-msvc-debug --output-on-failure -R "^rynui\.(composition_shader_layout|composition_d3d12_hidden|effect_paint_shader_closure)$"
ctest --preset linux-gcc-release --output-on-failure -R "^rynui\.(composition_shader_layout|composition_vulkan_private|effect_paint_shader_closure)$"
python tools/run_gallery_visual_acceptance.py --page modal --page popconfirm --page app --cases 4 --output out/gallery-final
```
