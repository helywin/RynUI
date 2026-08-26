## 1. C++20 UTF-8 String 值类型

- [ ] 1.1 在 `include/ryn/string.hpp` 与对应 source 中实现拥有内容的 `ryn::String`、UTF-8 strict parse result、带 byte offset 的 typed error 和 lossy replacement result，通过单元测试覆盖 ASCII、CJK、空文本、截断序列、非法 continuation byte、overlong encoding、surrogate 与越界 scalar
- [ ] 1.2 实现非拥有的 `ryn::StringView`、byte length/empty/view 和显式 `char8_t`/UTF-8 byte adapters，通过 API 测试证明 view 无分配借用且没有把 byte index 伪装成 Unicode 字符索引的 `operator[]`
- [ ] 1.3 实现只匹配 `const char8_t (&)[N]` 的 owning String literal 构造路径，使 `ryn::String title = u8"设备监控"` 与 owning String 属性无需 helper；不提供 `char`、runtime pointer、宏或 user-defined literal 路径，通过 compile/run test 验证 CJK bytes，并通过 compile-fail contract 验证未带 `u8` 的 literal 与 runtime pointer 不能隐式绕过 UTF-8 校验
- [ ] 1.4 从 `include/ryn/rynui.hpp` 导出 String API，并运行 public-header isolation/leak check，证明 consumer 只需 C++20 标准头且不包含 FreeType、HarfBuzz 或 SDL3 header
- [ ] 1.5 使用 `windows-msvc` preset 完成 Debug configure、build 与 CTest，运行 `git diff --check`；以英文 `feat: add UTF-8 string values` 提交，仅包含本阶段文件，fetch/rebase 后 push 并核对 remote SHA 等于本地 HEAD

## 2. Font 与 shaping 依赖合同

- [ ] 2.1 在集中 dependency lock 与 license 清单中固定 FreeType 2.14.3、HarfBuzz 14.3.1、Latin 验收字体与 Noto Sans CJK SC 的不可变 URL、SHA256、版本和 license，并以 lock/schema test 验证字段完整且 Git 未跟踪字体二进制
- [ ] 2.2 扩展 `BUNDLED` resolver：FreeType 使用发行源码 CMake、HarfBuzz 使用固定发行源码携带的 CMake target，关闭设计中列出的非必要模块并建立 `RynUI::FreeType`/`RynUI::HarfBuzz` canonical target；通过 `windows-msvc` clean configure/build 与 target contract test 验证单向依赖和选项
- [ ] 2.3 扩展 `SYSTEM` resolver，只接受版本匹配的规范 package target 与显式字体路径；通过隔离的 positive fixture 证明 canonical target 可链接，并通过缺 package、错误版本、错误 target、缺字体和无效 mode 的 configure-fail tests 证明绝不回退到 `BUNDLED`
- [ ] 2.4 实现验收字体资源解析：`BUNDLED` 下载到 build/cache 并校验 SHA256，`SYSTEM` 校验调用方提供的两个文件；通过测试证明首选 Latin 字体缺少目标 CJK glyph、后备字体覆盖它，且运行时不发生网络下载或系统字体扫描
- [ ] 2.5 运行 Windows/MSVC Debug 与 Release build/CTest、dependency license 检查和 `git diff --check`；以英文 `build: add text dependencies` 提交本阶段，fetch/rebase 后 push 并核对 remote SHA

## 3. FreeType Font Runtime

- [ ] 3.1 建立 `ryn_font` target 与私有 FreeType adapter，实现 library、字体 bytes、face 和 pixel-size 配置的 RAII 状态机、阶段化错误与 generation-checked Font identity；通过故障注入测试验证每个初始化失败点只释放已取得资源且旧 identity 不访问复用 slot
- [ ] 3.2 实现 Unicode charmap 选择、正 pixel size 校验及 ascent/descent/line gap/units 转换，通过锁定真实字体测试核对 metrics 稳定且无 Unicode charmap、非法 face index 与零字号返回明确错误
- [ ] 3.3 实现 Unicode scalar coverage 查询、按声明顺序的 fallback chain 与 replacement/missing-glyph 诊断，通过 Latin/CJK 真实字体测试证明不同字体选择、重复结果稳定及全缺字路径可诊断
- [ ] 3.4 实现 `FT_Load_Glyph`/灰度 rasterization adapter，将正负 pitch 规范为单通道 coverage 并保留 bearing、advance 与 visible bounds；通过真实 glyph、构造的负 pitch seam、空格与零面积 bitmap 测试验证行方向和 metrics
- [ ] 3.5 实现以 Font generation、glyph id、pixel size 与 raster mode 为 key 的 glyph cache 及 Font Runtime owner-thread guard，通过计数测试证明重复请求不 rasterize，并通过错误线程测试证明 coverage、mutation 与销毁 fail-fast 且状态不变
- [ ] 3.6 运行 Font Runtime 全部单元/真实字体集成测试、MSVC Debug/Release CTest、public dependency leak check 与 `git diff --check`；以英文 `feat: add font runtime` 提交本阶段，fetch/rebase 后 push 并核对 remote SHA

## 4. HarfBuzz shaping、measurement 与基础换行

- [ ] 4.1 实现合法 `StringView` 到 Unicode scalar/规范化 UTF-8 byte range 的 decoder，并把 raw-byte 防御入口接到 String lossy result；通过 byte-offset 与 replacement 计数测试证明输出不越界且后续有效文本可继续处理
- [ ] 4.2 实现 Font fallback 分段和 HarfBuzz buffer/hb-ft adapter，为 run 设置或推断 segment properties，并把 glyph id、绝对 byte cluster、advance、offset 与 Font identity 转换为 RynUI 自有值类型；通过 Latin/CJK fallback 真实字体测试验证至少两个有序 run
- [ ] 4.3 固定可单调追溯的 cluster 行为并处理连字/中性标点，通过 ligature 与重复 shaping 测试证明 cluster 不丢失、不重复且第三方 handle/enum/error 不进入跨模块头文件
- [ ] 4.4 实现从 shaped advances、offsets、font metrics 与 line height 计算 text bounds、baseline 和 line box，通过空文本、相同 codepoint 数但不同 advance、bearing/offset 与确定重复测量测试验证不得按字符数估宽
- [ ] 4.5 实现显式 newline、无限宽单行及有限宽下的空白/CJK 基础换行，只在 HarfBuzz cluster 边界断行；通过 CJK 多行、Latin 空白、连字、超宽单 cluster overflow 和 mixed-direction capability diagnostic 测试验证范围边界
- [ ] 4.6 建立持久化 engine Text state 与 shape/measure 计数，使 content/font/size/line-height/constraint revision 只重算对应 Text，color/opacity revision 不触发 shaping 或 layout；通过 Signal 集成测试证明不重新执行无关 Component
- [ ] 4.7 运行 shaping/measurement/layout 全部测试、MSVC Debug/Release CTest 与 `git diff --check`；以英文 `feat: add text shaping` 提交本阶段，fetch/rebase 后 push 并核对 remote SHA

## 5. GlyphAtlas、Scene 与 shader

- [ ] 5.1 实现 1024×1024 `R8_UNORM` CPU atlas page、1px 清零 padding、确定 shelf allocator、最多 8 页和追加式稳定 entry，通过 non-overlap、边界、分页、超大 glyph、容量耗尽及 UV/page 稳定测试验证
- [ ] 5.2 把 rasterized glyph cache 接入 atlas key，记录新增 entry 的 dirty rectangle、row pitch 与 uploaded-byte 计划；通过首次 glyph、重复 cache hit、空白 glyph 和 Material-only update 测试证明只新增必要 region
- [ ] 5.3 定义平台无关 `GlyphInstance`、Glyph range 与 ordered Scene draw command，正确应用 baseline、bearing、shaping offset、clip、translation、颜色和 opacity；通过 Quad/Glyph 交错、多 atlas page 与空格测试验证实例位置和稳定 Z order
- [ ] 5.4 增加单一 HLSL glyph shader、coverage alpha/blend 合同及 ShaderCross DXIL/SPIR-V 产物规则，通过 shader reflection/layout test 验证 instance binding、sampler/texture slot 和两种 binary format，且应用 target 不链接 ShaderCross runtime
- [ ] 5.5 为 Glyph instance store 实现 geometry/clip 与 Material dirty range 合并，通过范围测试证明颜色更新不改 atlas、translation 不 shaping、多个不相邻更新不扩大为全量 upload
- [ ] 5.6 运行 atlas/scene/shader 全部测试、MSVC Debug/Release build/CTest 与 `git diff --check`；以英文 `feat: add glyph atlas and scene data` 提交本阶段，fetch/rebase 后 push 并核对 remote SHA

## 6. SDL3 GPU glyph pipeline 与 Windows 真实窗口

- [ ] 6.1 在 SDL renderer 内实现 atlas `R8_UNORM` texture、sampler、transfer buffer、局部 texture region upload 与逆序释放，通过故障注入和 upload recorder 测试验证 pitch/alignment、page 绑定、只上传 dirty rectangles 以及资源失败无泄漏
- [ ] 6.2 扩展 Renderer 按 Scene command 顺序切换 Quad/Glyph pipeline 与 atlas page，只合并相邻兼容 range；通过 recording backend 测试证明不跨 Z/clip/texture 边界重排并保持 coverage blend 合同
- [ ] 6.3 把 Text shape/atlas/instance dirty 接入现有按需帧状态机和诊断计数，通过可控 event/clock 集成测试证明新增 glyph 请求帧、纯颜色只更新 Material、稳定文本停止 submit、后续变化可唤醒
- [ ] 6.4 创建 Latin/CJK 真实窗口示例，使用直接构造 String 的 `u8` literal、锁定 fallback fonts、14px Ant Design 6.5.0 正文与次级语义色，输出 Font、replacement、fallback、shape、atlas、instance、upload、draw、submit 和 idle 计数；通过 snapshot/日志 contract test 验证示例输入与计数字段
- [ ] 6.5 使用 `windows-msvc` preset 完成 clean configure、Debug/Release build 与 CTest，在 D3D12/DXIL 真实窗口触发 content、color、constraint 和 resize 更新并正常退出；保存截图/driver/退出码/计数证据，人工核对 12–16px Latin/CJK 清晰度、fallback 与局部更新关系
- [ ] 6.6 运行 Windows 阶段回归、evidence 完整性检查与 `git diff --check`；以英文 `feat: render glyphs with SDL GPU` 提交本阶段，fetch/rebase 后 push 并核对 remote SHA

## 7. Linux 验收与 change 收口

- [ ] 7.1 在 WSL/Linux 使用 `linux-gcc` preset 完成 clean configure、Debug/Release build 与 CTest，验证 Ninja Multi-Config、锁定 BUNDLED 依赖、HarfBuzz/FreeType target 和 SPIR-V shader 均来自预期来源
- [ ] 7.2 使用 `linux-clang` preset 至少完成 Debug configure/build/CTest，确认公共 String、Font、Text、Glyph 与 renderer 代码不依赖 GCC 扩展；若环境缺少 Clang，必须保留未完成状态并记录可复现阻塞而不得声称通过
- [ ] 7.3 在 Linux/GCC/Vulkan 真实窗口运行与 Windows 相同的 Latin/CJK 示例，触发 content、color、constraint 和 resize 更新并正常退出；保存截图/driver/退出码/计数证据，核对 fallback、局部 atlas upload、稳定 Z order 与 idle 无持续 submit
- [ ] 7.4 运行隔离的 `SYSTEM` positive/negative contract suite 与 public-header dependency leak check，确认显式 package/font 输入成功、缺失或不兼容输入 fail-fast，且 `BUNDLED`/`SYSTEM` 都不使用 submodule 或 system-first fallback
- [ ] 7.5 运行 Windows 与 Linux 全量 CTest、shader/lock/license/evidence 检查、`openspec doctor --json`、`openspec validate --all --strict --no-interactive` 和 `git diff --check`，只在所有对应证据通过后勾选任务并记录最终验收摘要
- [ ] 7.6 确认 worktree 只包含本 change 相关文件且 README、AGENTS 与 architecture 职责未混写；以英文 `test: validate cross-platform text rendering` 提交最终验收，fetch/rebase 后 push 并核对 remote SHA 与 clean worktree
