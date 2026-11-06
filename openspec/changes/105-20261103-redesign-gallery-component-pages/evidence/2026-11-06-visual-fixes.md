# 全目录巡检修复结果

## 阶段一：栈压力与 MSVC 编译

Windows/MSVC `windows-msvc-release`、Ninja Multi-Config、最多 2 jobs，正式 preset 重建 Gallery 及 Signal/Prop/ThemeScope/Anchor/TransferComplete 五个测试 target。Anchor 已以显式类型递归遍历通过标准编译，未使用 `_CL_` 绕过；Gallery PE 栈保留仍为默认 1 MiB。

内部大值转交改为右值引用，公开 Signal/Prop 按值入口保持复制隔离；Theme 配置初始化返回后再挂载子内容，Transfer 轻量重入检查在重型刷新帧之外；验收宿主的配置更新同样在挂载前完成，避免常驻多份完整 ThemeConfig。

`ctest --test-dir out/build/windows-msvc -C Release --no-tests=error -R "^rynui\.(signal|prop|theme_scope_component|anchor_component|transfer_complete)$"`：**5/5 通过**。新增大值重入/复制隔离、25 层透明 Theme、Anchor 前序/继承/非法 key/数值/128–129 层边界断言，旧身份与失败回滚断言保留。

原隐藏 GPU 入口四组复跑：Transfer **exit 0 / 15 图**；Avatar **exit 0 / 16 图**，覆盖两个样例组；Cascader 已完成首次布局和 4 张捕获，随后 **exit 1 / ComponentId is stale or invalid**，该新暴露问题仍待修复。三入口此阶段均未再返回栈溢出，**不据此宣称 Cascader 整页通过**。原始失败、调试器异常栈、图像仅在 out。

本阶段为增量构建与隐藏 Windows/D3D12 回读；没有 Linux、桌面 swapchain、系统 IME/AT 或高刷验收结论。

## 阶段二：视觉与关联生命周期

| 编号 | 修复与复核 |
| --- | --- |
| B01–B03 | 共用栈压力修复后，三个入口均可完成四组采集，Avatar 两个样例均有图像。Cascader 后续 stale ID 来自工具对内嵌模式不存在的 selector 发起激活；仅过滤无效目标，保留真实失效错误。 |
| V01 | Search 的局部 Dark/Compact Theme 配合实际 Card 背景，文字和输入内容恢复可读。 |
| V02、V11 | Dropdown/Popover 先测量内容自然宽度，再按真实 viewport 约束重测；ColorPicker 的 panel、presets 与 extra 按纵向排列。自定义浮层不再撑满窗口，extra 不再覆盖色板。 |
| V03 | Calendar 窄屏样例使用可容纳的单元格 padding 和“详情”按钮；备注输入保留原值及编辑能力，正常水平滚动，不再只剩空边框。 |
| V04 | Pagination 内部主题分别清除三种尺寸的 Button inline padding，480 宽的末页 13 完整显示；兼容补充两个可选 typed padding token，默认值不变。 |
| V05 | Tabs add/close/more 使用图标 slot；隐藏项通过 reactive Menu 更新，滚动时保留更多按钮及其文字场景身份。 |
| V06 | 零宽 Sider 保留窗口绘制层，但不截断文档 translation；触发器跟随滚动，服从真实祖先裁剪。覆盖可见 glyph、实际点击/Space、离屏恢复和 resize 身份。 |
| V07、V08 | Masonry 富内容改为显式纵向流；Spin 示例给保留输入与按钮之间留出 loading 内容空间。保留 Spin 现有遮罩和交互合同。 |
| V09、V10 | ColorPicker 色面共享完整形状的外缘 AA，内部切片由半开 clip 单次覆盖；通道编辑不显示步进按钮并增加可读宽度。半透明 source-over 回归覆盖 4 种 scale × 4 种物理相位。 |
| V12 | Carousel 示例继承实际主题背景，Dark 下 panel 文字恢复对比度。 |
| V13 | Breadcrumb 导出富标题首条真实文字 baseline，并使同一行条目/分隔符对齐；允许富内容保持自己的高度。 |
| V14 | Upload 卡片/圆形列表动作使用图标与 Tooltip；普通列表保留完整动作文本，卡片动作不再在单词中间折行。 |
| S01、S02 | Outlined Input 的边框改为真实 outline，避免饱和色填充透过禁用背景；垂直 Form 标签从控件起点对齐，水平 labelAlign 规则保留。 |
| S03、S04 | Input 说明与对应编辑器成组；句号前关闭优选断点，保留极窄宽度的 grapheme 紧急折行，并未实现完整 UAX #14。默认 HSB 触发器精简到两位小数，模型、编辑器、自定义 text renderer 和回调精度保持。 |

受影响回归还修复了三个既有边界：Select 顶层浮层 padding 消费滚轮；Breadcrumb 移除编辑器前发布失焦并保留 owner 销毁/抛异常合同；OTP 缩短长度先完成失焦，尊重回调的外部焦点请求，抛异常后仍完成格子清理并解除 resize guard。

正式 `windows-msvc-release` 相关 target 构建成功；定向 CTest **62/62 通过**，包括 7 个新增 Gallery 真实宿主/色面合同及既有 Tabs、Popover、Dropdown、Breadcrumb、Form、Upload、Input、TextArea、OTP、Layout、TextEngine、Theme 合同。旧断言未放宽；ColorPicker 逻辑色面检查随实际存储从 Quad 改为 RoundedEffect，仍检查 hue 更新确实改变已发布色面。未执行全量 CTest 或重复 Debug 平台通用验收。

## 最终原生复验

`python tools/run_gallery_visual_acceptance.py --output out/gallery-fix-final-20261106`：**67/67 入口 exit 0、97 个样例组、1,497 张局部/状态图**。配置继续为 Default/1440、Dark/960、Dark+Compact/728、Default/480，高 900、实际 scale=1.5，Windows/MSVC Release、SDL/D3D12/DXIL、系统字体、隐藏 GPU 回读。

该全目录批次使用视觉修复后的构建；其后完成 Tabs retained menu 和 OTP 失焦边界的最终补修，再以标准 preset 重链最终 EXE，定向复跑 Cascader、Transfer、Avatar、Tabs、OTP、Breadcrumb 六入口：**6/6 exit 0、138 张图**，各四组配置。按每入口最新批次合并仍为 **67 个入口、97 个样例组、1,497 张图**，不把补拍重复计数。Avatar 两个样例均捕获；Cascader 不再出现首轮工具的 stale selector 错误。

对照修复前原尺寸问题图，复核了上述 B/V 项以及 S01–S04 的对应外观；不是对所有生成状态逐张宣称视觉通过。Typography 保留两条往返裁图高度 1px 差异告警，与首次巡检相同，没有空白或像素差异告警。未覆盖完整 hover/拖拽/动画、全部 DPI/实际显示器、桌面 swapchain、OS IME/AT、Linux 或 240Hz 体验；原 tasks 6.1/6.2、7.1 保持未完成，未扩展本轮验收范围。

最终 Gallery 的 PE stack reserve 为 **1,048,576 bytes**；`CL`、`_CL_` 均未设置，标准 Release flags 为 `/O2 /Ob2 /DNDEBUG`，没有提高栈或保留 Anchor 编译绕过。原始失败历史和所有运行产物只在 out。clang-format 22.1.3、Gallery 源码摘录生成检查（101 条）、OpenSpec doctor、strict **107/107**、`git diff --check` 通过；只提交源码、回归及本精简结果。

实现提交 `fedcb76c` 已 fetch/rebase/push，并核对远端 main 一致。本次 11.1–11.5 已完成，原有三个独立平台项保持未完成；没有 archive 或恢复暂停的其他调优任务。
