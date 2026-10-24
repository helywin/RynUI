# Spec Delta

## Purpose

为 RynUI 使用者提供可从目录直接到达的原生导航交互示例，让层级激活、受控流程、富内容编辑及恢复行为可以实际操作，同时保留现有 Gallery 示例身份和诚实的支持范围。

## ADDED Requirements

### Requirement: Breadcrumb interactive trail

Gallery SHALL 提供唯一 key 的动态层级、原始 key/index 激活反馈、图标和自定义分隔符，以及改名、隐藏、禁用和外部恢复操作；当前非导航项和无导航处理器的展示项内启用的原生子控件 MUST 保持编辑和独立激活。

#### Scenario: Retained trail editing
- **WHEN** 使用者编辑富标题、激活祖先并改变层级可见和禁用状态后恢复
- **THEN** 反馈包含原始 key/index，草稿和子控件身份保留，当前项和子按钮不意外发出导航请求

### Requirement: Controlled Steps workflow

Gallery SHALL 展示受控步骤、实际请求反馈与上一步/下一步/重置、横纵向、两尺寸、状态和可见/禁用控制；键盘方向键与 Home/End MUST 只移动焦点，Enter/Space 才提交，富说明子控件独立编辑和激活。

#### Scenario: Original indexes and keyboard requests
- **WHEN** 使用者隐藏或禁用中间步骤，使用方向键和 Home/End 后按 Enter/Space
- **THEN** 焦点跳过不可用步骤，请求仍使用原始索引，受控值与反馈同步，子编辑草稿保留

### Requirement: Catalog reachability and retained Gallery

Gallery SHALL 从现有 Breadcrumb/Steps 目录按钮跳转至真实样例，在三主题和宽窄有限视口中显示可操作内容；旧样例 ID、顺序、功能与非目标支持条目 MUST 保持。

#### Scenario: Finite viewport navigation
- **WHEN** 在 Default/Dark/Compact、1200 或 560 宽和 900 高视口点击目录项目并操作样例
- **THEN** 实际文字、表面和命中受有效裁剪，留有空间时外扩焦点可见，普通属性和 Theme/layout 更新不重建组件或编辑器
