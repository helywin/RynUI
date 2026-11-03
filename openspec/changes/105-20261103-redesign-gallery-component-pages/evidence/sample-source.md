# 实际 C++ 与公开 API 入口

2026-11-03，实际 Linux / `linux-gcc` Release / Ninja Multi-Config / BUNDLED。

- 构建 `rynui_gallery_sample_page_tests`、`rynui_gallery_source_panel_tests`：通过。定向 CTest `gallery_sample_page`、`gallery_source_panel`、`gallery_sample_source_contract`、`gallery_sample_source_generated` 共 4/4 通过；源码面板失败项修正后单独复核，没有重复整组。
- 67 组源码直接来自实际编译的样例函数，包含真实文件/行号和 add_* 依赖；最长片段 15539 UTF-8 bytes。C++ 消费者核对所有工厂的函数、头文件与 UTF-8，未知样例返回空，不制造未实现代码。
- 公开 Props、slots、常用成员、ThemeConfig Token 入口来自现行头文件；覆盖 forwarding header、PropsBase、共享配置类型和 Token alias。没有组件 Token 的条目保持空映射。词法测试覆盖注释、普通/原始字符串、字符中的括号及截断源码；生成可重复。
- 原生 Collapse/TextArea 面板通过键盘展开、关闭时焦点门控、只读原文、窄宽度下实际横向 scroll offset、编辑器身份保留和复制内容核验。面板未接入正式窗口，shell 任务保持未完成。
- CPU 支持库从 examples 开关中提取，供 HEADLESS 测试使用。Linux 下以 `linux-gcc` preset、独立 out 构建目录和 HEADLESS/RECORDING/EXAMPLES=OFF 覆盖成功 configure；本阶段没有执行该独立目录的编译，也不代表 Windows/MSVC 通过。

生成源码副本和全部原始输出只在 `out/`，无新增依赖或网络运行时。上述测试为模拟文本平台/CPU scene，不代表真实 GPU 或系统输入服务。格式/OpenSpec/diff 结果随本阶段提交记录。
