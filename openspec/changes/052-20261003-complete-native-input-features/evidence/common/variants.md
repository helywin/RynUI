# 四变体平台通用验收

环境：Windows 11、MSVC、Ninja Multi-Config、windows-msvc-headless Debug/Release。

input_variant 覆盖明暗 Theme × 四变体 × 三尺寸 × 三 status，验证等高、背景/底边/outline、半透明 Filled 无重复混色、pointer/keyboard 焦点、disabled、Token override 和 Compact 接缝。reactive 变体切换保留 editor、选择、IME stamp、scene ID、slot 数及挂载次数。Password/Search 透传变体；Search 非 Outlined action 类型及 Compact 连接合同通过，action 最终背景/底边仍由任务 4.1 完成。

Debug 10/10（8.72 s），Release 10/10（5.88 s）：input_variant、input_component、input_public_api、password_component、search_component、theme_algorithm、theme_runtime、space_compact、space_features、rounded_effect_store。命令使用相应 headless preset，构建 rynui_portable_* 目标并以对应 CTest 正则筛选。

Search 纯主题色变更不增加 mount/shape/measure/topology。新增 rounded_effect_store 合同验证窗口外、祖先裁剪及显式隐藏 effect 的 material 更新不重打包，重新进入可见范围后保留更新值。Theme golden 更新包含新增 Input 色组/focusWidth，并经算法测试验证。

这是平台通用 CPU/scene 合同证据；Windows GPU 图像和 Linux 平台验收分别由任务 6.1/6.2 记录。
