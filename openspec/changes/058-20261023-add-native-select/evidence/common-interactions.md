# 单选交互阶段验收

2026-10-23，Windows，MSVC x64 / Ninja Multi-Config，平台通用 preset `windows-msvc-headless`。

- Debug完整CTest103/103，206.12秒（input_scene_allocation206.10秒）；Release完整CTest103/103，11.68秒。此处只记录实际运行，不作为跨版本性能比较。
- 真实组件覆盖：selector点击、保焦候选点击、三尺寸/四变体/status、prefix/suffix文字图标、清空、单一Tab入口、禁用项跳过、Home/End/Enter/Escape/Tab、controlled值/打开状态拒绝与echo、未知值和动态选项、空结果、loading/disabled、自动IME组合/取消/提交、ref与生命周期。
- 动态选项按下捕获后替换，确认旧capture清理，随后up不会选中新行；重复选项在发布前拒绝，不改变已挂载候选。
- 100项列表滚轮与活动项reveal、TextArea原滚轮路径、viewport边缘翻转/缩窄、父content clip与独立菜单内部clip、失活隐藏、visible idle、颜色更新不重测和兄弟scope不重跑均通过。
- onChange/onOpenChange/onSearch/onFocus/onBlur内销毁后，无残留ref、interaction、retained surface或输入会话。销毁流程先使scope失效，Select不再从失焦回调访问正在清理的弹层；Input销毁编辑器前显式终止自己的活动session。
- 窗口层继续保留Component/theme/scope父子关系，但在Node content clip链建立边界。菜单内部viewport另外裁切文本，未修改renderer/GPU ABI。
- Select保留Input配方。内部Theme转换active outline颜色时保留所有shadow layers；ShadowList增加显式span构造，不压扁多层shadow。
- clang-format22.1.3检查468个自有文件0失败；doctor healthy；strict validate58/58；diff check通过。5份golden的旧组件Token值保持不变。

此阶段未运行真实D3D12/Vulkan窗口；Gallery和Windows/Linux平台任务仍独立待验收。自动composition事件不替代人工系统IME。
