# Context

导航当前使用透明背景的 Default Button，但 Default Button 的边框层是完整圆角面，悬浮时与前景同时变蓝。Gallery 手动主题按钮直接更新 theme Signal，却绕过更新窗口背景的 helper。组件目录子项由静态 Text 声明。Password/clear action 现有 PressableBehavior 和 focus 路径应继续复用。

官方参照：[Icon API](https://ant.design/components/icon-cn/)、[图标设计](https://ant.design/docs/spec/icon/)、[Ant Design SVG icons](https://github.com/ant-design/ant-design-icons)。图标应随文字大小、颜色变化。锁定上游资源的实际版本与哈希写入统一 dependency lock。

# Goals / Non-Goals

目标是建立真实图形图标基础，解决所有已报告的导航/主题问题，并维持局部更新、输入合同和滚动稳定性。非目标见 proposal；不依赖系统字体中的符号、Unicode 近似图形或运行时网络资源。

# Decisions

1. 首批包含 EyeOutlined、EyeInvisibleOutlined、SearchOutlined、CloseCircleFilled、MenuOutlined、SunOutlined、MoonOutlined、UserOutlined、LockOutlined。保留官方 SVG，离线转换为内嵌轮廓容器，交由现有 FreeType 路径按实际 DPI 栅格化。该容器只作为私有资源，不加入系统文字 fallback chain；不会因用户字体选择而变成缺字方框。
2. 公开 `IconProps` 提供 typed name、reactive 属性、语义 tone 与 LayoutStyle；尺寸/颜色来自 Theme 与 slot semantic typography/foreground。内部共享 retained glyph 场景及 atlas，颜色变化只更新 material，换图标不重跑父组件。字体资源按字号/DPI 缓存，销毁顺序与 TextSceneService 一致。维护工具用于生成，不是正式构建依赖。
3. Input affix action 挂载 typed Icon，保留点击、键盘激活、焦点、禁用和可见性合同，统一为主题驱动的普通/悬浮/禁用前景，并显示键盘焦点。Search 自定义按钮文案保持原 slot；只有默认图标入口替换。
4. 增加 ButtonType::Text，边框和阴影在所有状态均不绘制，悬浮/按下背景及前景由主题派生。导航使用该类型；选中底色也取 Theme，不再依赖透明 Default Button 的偶然组合。
5. 导航 target 扩展为 component identity；锚点按离线目录稳定 ID 建立和重排，保留 generation 失效保护、宽窄切换和左右独立滚动。点击任意组件子项定位对应卡片；被筛选隐藏时恢复全部后再定位。
6. 统一 Gallery 主题更新入口，所有入口同步 clear color 与根 Theme。主题示例可明确展示局部主题，但全局正文、导航、图标和输入辅助操作采用当前根主题。固定顶栏切换亮暗，主题切换不重挂载内容。
7. 滚动条 controller 输出轨道/滑块的 hover、pressed、dragging 状态，ReferenceSurface 按当前 Theme 解析视觉；颜色变化仅更新 material。指针移出、松开、cancel、窗口失焦和宽窄切换必须清理状态，拖动移出轨道后仍保留捕获及拖动反馈。

# Risks / Trade-offs

使用内嵌轮廓容器能复用已经验收的 glyph 缓存和 DPI 路径；与通用 SVG renderer 相比暂不支持多色/任意 SVG。转换必须可重现并检查空轮廓、固定宽度、所有图形可识别。新增文字按钮影响 enum 验证和 token 测试；现有 Default/Primary/Danger 行为不得回退。

# Validation

平台通用测试使用 Windows `windows-msvc` Debug：资源来源/生成合同、typed API、图标缓存/生命周期/颜色局部更新、Input/Search 行为、Button Text 状态、导航组件锚点和主题背景同步。Windows 独立使用 Release/D3D12，检查亮/暗首页、悬浮导航、图标及密码切换，复测连续拖动和输入。完整 CTest 运行一次，最终更改按受影响范围复测。

# Migration Plan

依次提交规划、图标资源、Icon 组件、控件接入、导航与主题、Windows 与集成证据。英文 conventional commits；不主动 push、PR 或 archive。当前 OpenSpec CLI 拒绝数字开头的项目约定名称，因此规划文件按既有 schema 手工创建，使用支持该名称的 strict validate；不以此跳过验证。
