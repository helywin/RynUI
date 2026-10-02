# Gallery 与共同集成

2026-10-02，实际 Windows x64 / MSVC / Ninja Multi-Config。

- windows-msvc-headless Debug/Release 完整 build、CTest 46/46，122.54 s / 14.97 s；包含实际 HEADLESS 和 Core include/link guards。
- windows-msvc Debug/Release Gallery frame、reference catalog、font runtime、Switch features/public API 5/5，30.24 s / 5.53 s。
- Gallery 新增三个 Switch 与 ref 聚焦按钮：文字双分支、Small 图标/RTL、独立紫色组件主题。102 live samples、85 stable IDs、56 Theme content runs；Switch slots 保留挂载。
- native implemented 目录引用 046 和组件测试证据；DOM/React/HTML/CSS 与 Web value/defaultValue 别名保持范围外，Linux 原生验收独立 pending。
- Gallery 实际复现 MSVC 默认栈耗尽：拆分新增样例挂载函数，ThemeScope create/constructor 借用 const 配置引用并只复制到自身成员，移除沿调用链的多余大型参数副本；完整 Gallery Debug 测试恢复通过。
- Text 空祖先裁剪区域直接跳过，实现屏幕外 Switch 内容延迟生成；滚动断言保留无裁剪文本几何不变和全部文字不重建 glyph 几何，对移动的内部裁剪框允许局部 patch。
- generator --write/--check/--self-test、clang-format 22.1.3、402 自有源文件格式检查、OpenSpec doctor、46/46 strict 与 git diff --check 均通过。

完整 headless 日志：out/046-switch-stage3-headless.log。共同结果不等同于真实窗口/GPU 或 Linux 原生验收。
