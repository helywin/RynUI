# Typography 2.1 / 2.2 验收

2026-11-04，Linux x86_64 / GCC 16.2.1，正式 Ninja Multi-Config presets。

- `linux-gcc-headless` Debug / Release：相关 14 个 target 构建成功；下列定向 CTest 各 **17/17** 通过。覆盖 rich runs 的实际字体尺寸与字体解析请求、Arabic/Hebrew 字形和跨样式 Arabic 邻接、整段 UBA/Unicode conformance、字簇/emoji ZWJ、换行与 RTL 省略后缀、语义装饰/颜色、Theme/reactive 局部更新、全文复制/HTML 转义、编辑保留、空内容、Gallery 页面与源码/目录、Core/backend 依赖边界。
- `cmake --build --preset linux-gcc-debug --target rynui_token_gallery rynui_text_clipboard_tests -j 2` 成功；`ctest --preset linux-gcc-debug -R '^rynui.text_clipboard$'` **1/1** 通过。此项是 SDL 平台 API mock，覆盖 UTF-8 双格式、payload 替换/cleanup/失败所有权、分配失败、错误线程、NUL/超限，以及 CF_HTML header/fragment 的字节偏移；没有创建系统窗口或调用真实系统剪贴板。
- `python tools/update_ant_design_gallery_catalog.py --check --self-test` 通过；Gallery 源码/API 合同 **67 samples** 通过。`python scripts/format-code.py --check --clang-format /home/jiang/code/RynUI/out/linux-validation/format-tools/bin/clang-format`：clang-format 22.1.3，**845 files / 0 failures**。OpenSpec doctor 健康，strict 全量 **106/106**，`git diff --check` 通过。

定向 CTest 命令（两种 configuration 使用同一过滤器；所有构建最多 `-j 2`）：

```sh
typography_test_filter='^rynui[.](portable[.](typography_component|typography_interaction|text_engine|text_state|text_scene_service|text_clipboard_command|text_component|text_caret_map|text_component_frame|backend_boundaries)|bidi_analysis|bidi_shaping|bidi_layout|ant_design_reference_catalog|gallery_sample_page|gallery_sample_source_contract|gallery_sample_source_generated)$'
ctest --preset linux-gcc-headless-debug -R "$typography_test_filter"
ctest --preset linux-gcc-headless-release -R "$typography_test_filter"
```

原生 Gallery 已增加实际 mixed runs、双行省略和 HTML copy 示例，保留既有复制/提示/TextArea 编辑。目录仅将 Typography 的已实现原生合同标为 implemented；DOM/CSS/React、任意 HTML 输入与 URL 导航明确排除。

Windows 锁定 SDL backend 忽略 HTML MIME，已在私有平台适配补充标准 `HTML Format` 与独立 User32 依赖，Core 保持平台无关。CF_HTML 字节格式已在上述 Linux mock 测试验证；**Windows/MSVC 编译、真实 Windows/Wayland 剪贴板消费端互操作、GPU/窗口输入均未在本阶段实测**，不勾选 7.2 / 8.1 / 8.2，不以 HEADLESS/mock 结果代替这些证据。原始日志和二进制只存 ignored `out/change106-typography/` 与 `out/build/`。

按用户最新要求仅创建本地 T3 worktree commit，不 push T3 分支，不删除远端分支；由父线程按通用、布局顺序集成 main。
