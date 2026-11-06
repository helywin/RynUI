# 反馈组件、浮层与 i18n：Windows 阶段结果

基线为锁定的 Ant Design 6.6.5。本阶段合入反馈组件、App/Notice 活上下文、统一浮层输入域及组件文案，修复实际构建、输入、清理和截图过程中发现的问题。

## 实际自动验证

- Windows/MSVC，`windows-msvc-headless`，Ninja Multi-Config，最多 2 compiler：65 个相关 target；Debug **182/182**、Release **182/182** 通过。使用 CTest 按这些 target 的实际测试入口执行，未重复无关全仓矩阵。
- 覆盖原组件与新增合同：浮层分配失败恢复、焦点/命中/租约、App 代际与 future、通知时钟、反馈动画和停帧、默认文案优先级、双窗口/嵌套配置、富编辑器保留、Picker 原始日期键与 Tab 顺序、Range 候选语义及回调重入。Editor 入口完整执行旧 13＋新 15 组。
- 五套 Theme golden 的旧值/前缀保持检查通过；新增 Skeleton/Popover 字段由实际程序生成。Gallery source/API/token 检查 **109 个样例通过**；目录生成、自检、文档示例、clang-format 22.1.3、OpenSpec doctor/strict **108/108** 与 diff 检查通过。
- `windows-msvc-release` Gallery：真实 SDL/D3D12、DXIL、系统字体、hidden GPU readback；Default/1440、Dark/960、Dark+Compact/728、Default/480，实际 scale=1.5。按每入口最近成功批次合并，**28 个入口、1,520 张局部/状态图**完成自动交互与捕获，包含有限动画帧、滚动离开/返回与弹层操作。

已复核选定宽/窄 contact sheets 和问题局部图。Popover 右键滚动的四配置均验证真实溢出及底部操作；Skeleton.Image 的图标尺寸/颜色已在修复后重拍确认。Alert 两个往返差异告警仅位于 20/22 像素高的跑马灯文字带，属于动画相位变化，保留告警记录。捕获数量不作为逐图黄金比对通过数量。

## 本阶段修复

- Cascader 多实例同步刷新使用堆上主题配置与独立发布入口，消除已复现的大栈帧叠加；不增加栈保留、不改公开 Signal 值语义。
- 收尾额外启用的 `gallery_sample_page` 在 Debug 暴露 Transfer 同类的大主题栈帧；采用同样的配置存储/发布隔离后，真实 Gallery 宿主及 Transfer/集合定向回归通过。该后续修复只扩展相关检查，不重复无关的 182 项。
- 浮层/input batch 避免 MSVC Debug 的 noexcept vector 分配中止；实际逐分配失败扫描、恢复、pointer/capture/focus 和清理回归通过。
- Range 按端保存日期 mode/first-day 与时间 precision；最新 min/max/predicate 继续校验。成功 stage 与父尾部 supersession 分开，避免重复提交、误关闭和新草稿被旧回调覆盖。同一 TimeRange 两端保持共同焦点边界。
- 日期/时间提交回调切换语言后开始的新草稿使用新的格式快照；同一新编辑中的后续字符继续冻结。合法等值请求不依赖被抑制的 value 通知结束编辑。
- 通知失活时保持真实到期时钟，只停止动画持续帧；hover/隐藏来源的暂停合同分别保留。Spin 内容 range、Drawer resize 量测、Tooltip 清理和 Skeleton 图片图标等实测失败已修复。

## 保留的边界

107/108 继续跟踪 Progress B2/C 与非 Round cap 的数值/GPU认证、Card 任意后代圆角遮罩、完整 Drawer 开合及 Modal/Drawer/Popconfirm 动态服务、未合入的 Table/Tour 补齐，以及数字/Form i18n、Gallery 全局双语。nonRound capability 保持关闭。

本轮未增加 Linux/Wayland/Vulkan、系统 IME、OS accessibility adapter 或全部物理 DPI 的验收。原始日志、截图、二进制和运行索引仅保存在忽略的 `out/`；本记录仅保留结果摘要。

测试夹具按实际窗口顺序先 dispose 组件，再释放 InputHost。调试中观察到单独提前析构 InputHost 会触发 Select 内部 stale ID；该拆卸顺序边界仍待后续生命周期修复。
