# Design

## Context

当前根 Flex 的第一个孩子是导航列、第二个孩子是正文列，`GalleryDocumentViewport::apply_subtree_translation` 却作用于整个根。导航用 `Space(wrap=true)` 包装 section/category/filter 按钮，缺少组件级层级。Ant Design 6 官方[组件总览](https://ant.design/components/overview/)及[Layout 文档](https://ant.design/components/layout/)给出顶部站点栏、左侧按 General、Layout、Navigation 等分组的纵向导航与固定 Sider 模式；Gallery 以锁定的 6.6.5 目录数据为条目来源。用户明确要求滚动区有可见滚动条。

## Goals / Non-Goals

**Goals:** 顶栏固定；宽窗口中左栏与正文互不跟随滚动，左栏自身可滚到末尾；左右两列有可见轨道和滑块，可滚轮、拖动及点击轨道；导航层级、宽度和选项顺序与官方组件文档接近；保留现有 section/category 跳转、筛选和输入行为；窄窗口仍可到达全部内容。

**Non-Goals:** 不实现任意嵌套的公开 ScrollArea、GPU scissor/ClipPrimitive、水平滚动条或完整 Ant Design Menu API；不把 Gallery 布局改造描述为 Layout/Sider 公开组件已完成。

## Decisions

1. **内部滚动区先行。** 提取独立的 `GalleryScrollRegion`，保存视口范围、内容长度及 offset，校验有限值、夹紧滚动，并把偏移应用到指定 Node 子树。`GalleryDocumentViewport` 复用此机制并继续负责 section/category 锚点及 resize 恢复。左栏使用第二个状态实例，不复制锚点语义。各自的 root identity 用 generation 校验。
2. **滚动条作为先行组件。** Gallery 内部的 typed Props 装饰组件绘制滚动轨道与滑块，Theme/Component Token 决定颜色与圆角。滚动区状态按 `viewport/content` 比例给出滑块高度和位置，并设置最小可抓取长度。指针按下滑块后捕获拖动直到抬起/取消；点击轨道按视口步进；滚轮与程序跳转更新同一 offset。内容短于视口时滑块填满轨道且不能滚动。
3. **顶栏与宽窄模式。** 固定站点栏高于滚动区，宽布局两列只平移正文子树或导航子树；滚轮使用窗口逻辑坐标判定所在列。顶栏在场景绘制顺序中覆盖正文和导航的越界部分，顶栏区域的指针不透传到滚动内容。窄布局的导航位于正文上方，使用单列共同文档滚动及一条滚动条。Resize 后更新轨道、夹紧 offset，所有区域仍可到达。
4. **导航内容。** 使用目录的类别和条目顺序，类别标题为一级，组件名称为缩进的第二级；文档入口保留为独立组。交互继续采用已实现 Button，导航组通过 Theme/Component Token 调整为轻量文本式外观，不以 `LayoutStyle` 改写 Button 的内部视觉。类别仍跳到正文对应位置；组件条目先展示目录层级，不宣称不存在的单组件文档页。
5. **边界与可见性。** 宽布局左右区域占据不同 x 区间，顶栏占据独立 y 区间。当前窗口级 clip 继续限制窗口外绘制，顶栏的非透明面覆盖越界部分；输入命中随 Node translation 更新，并对顶栏区域拦截透传。未来公开 ScrollArea 需要祖先裁剪贯穿 GPU 绘制、文字、effect 与 HitTest，不复用本次布局特例。
6. **验证。** 平台通用 headless 测试检查滚动条几何/拖动映射、两列根身份、滚动隔离、导航项目目录覆盖、宽窄切换和不 remount。Windows 正式 `windows-msvc`/Ninja Multi-Config/MSVC Release 跑真实 D3D12 滚动、拖动、主题和输入模式；记录本次导航节点增加对首帧与滚动的影响。Linux 不重复验证平台通用逻辑，本 change 不声明 Linux 窗口验收。

## Risks / Trade-offs

- 顶栏覆盖与两列横向分离适用于本次 Gallery；未来公共 ScrollArea 必须先有祖先裁剪贯穿绘制、文字、effect 与 HitTest 的合同。
- 目录条目从静态正文扩展到导航会增加 Button/Node 数量，需观察首帧、draw 和滚动 CPU；若增长明显，应在后续阶段改为轻量导航项或虚拟化。
- 文档锚点此前用 ReferenceSurface 固定序号取得；改动导航结构时必须用稳定目录身份或核对序号，避免静默跳错。

## Migration Plan

先提交规划，再实现内部滚动区与单元测试，再调整导航与根选择并跑 headless 测试，最后运行正式 Windows 真实窗口和集成校验。所有提交使用英文规范前缀；不自动 push、PR 或 archive。

## 用户反馈修正

用户报告滑块持续拖至中段偶发崩溃，并要求重新整理排版。既有跳至终点的拖动验收不能覆盖持续移动时的事件和帧交错；新增沿完整轨道往返的验收，先记录复现与根因再修复。排版继续以官方组件页为参照，统一导航行高与左对齐、正文最大宽度与留白、标题层级和区块间距；布局与滚动轨道共享尺寸定义，防止视图位置与命中坐标漂移。先完成基础组件/几何能力再接入 Gallery，并用实际窗口截图核对宽窄布局。

- 窄窗口保留导航与正文连续滚动，但初始定位到正文概览；固定顶栏的目录按钮回到导航起点，选择文档后继续跳转。宽窄切换从 offset 0 也恢复原 section。
- 标题、来源和设计理念使用无状态标记的文档表面。组件目录中的长支持范围使用整行卡片，规划条目使用紧凑网格。原始来源、scope/evidence 数据继续保存在离线目录中，界面显示紧凑的来源和参考记录数量。
- 真实 Release 复验发现额外的启动访问冲突：CMake 缓存的中文 `/showIncludes` 前缀乱码使 Ninja 的 `main.cpp` 依赖为 0，导致新旧 `TokenGalleryDefinition` 布局混用。Windows preset 设置 `VSLANG=1033`；缺少英文资源时仍可能回退到中文，因而补充真实输出代码页探测。清理重建后验证真实头文件依赖及增量重编译。
- 图像验收使用相同 scene、shader 与 GPU resources 绘制至同格式离屏目标，导出 BMP；不读取 swapchain，不改变正常窗口提交路径。定位后等待 350ms，使导航颜色过渡完成。导出结果用于检查首页、中段和窄布局。
