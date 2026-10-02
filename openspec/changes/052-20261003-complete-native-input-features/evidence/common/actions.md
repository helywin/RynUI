# 单行输入操作验收

环境：Windows 11、MSVC、Ninja Multi-Config、windows-msvc-headless Debug/Release。

14/14 CTest：Debug 10.89 s，Release 7.30 s。覆盖 input_actions、input_count、input_variant、input_component、input_public_api、password_component、search_component、search_public_api、icon_component、button_component、space_compact、typography_interaction、pointer_route、focus_lifecycle。

清空合同包括 disabled 保留图标且取消已捕获按压、reactive 双层 vector 图标、IME 取消、一次空值编辑及 onChange/onClear 顺序，以及 onChange 同步卸载后的回调生存期。

Password 合同包括 reactive visibilityToggle 不重建、隐藏不保留 suffix 空隙、prefix/suffix、自定义 vector renderer、Hover 每次进入切换一次、Click/键盘操作、toggleFocusable 资格变化取消待执行键盘按压，以及鼠标显隐保留选择和 IME。默认隐藏/显示图标按锁定上游纠正为 EyeInvisibleOutlined/EyeOutlined。

Search 合同包括受控回写拒绝清空时仍发送空候选及 Clear 来源、loading 只拦截提交、共用 Props、reactive searchIcon、自定义内容优先、Filled 普通/hover/pressed material、Underlined Text action、Small 共同最小高度和图标 action 最小方形尺寸。更新 Input Theme 颜色不重新 shape、挂载或重建；增大 Small Input 字号同时更新两侧高度。动作鼠标焦点只在所属 Input 已聚焦时保留 Input 焦点/选择/IME，其他情况下 Button 正常获取焦点。

上游源码合同保存于 ../source-contract.json：Ant Design 6.6.5 的锁定 commit、Search.tsx、style/search.ts、Password.tsx 的 SHA256 及本地原生映射。未引入组件私有 renderer/平台上传路径。

对应命令：以 common Debug/Release preset 构建上述 rynui_portable_* 目标，并运行相同名称的 CTest 正则。完整 Gallery 和真实窗口/GPU/OS 验收由后续任务独立记录。
