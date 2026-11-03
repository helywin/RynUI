# Windows 组件页反馈回归

2026-11-03；本轮仅处理用户新增的四项组件页反馈，不替代原任务 6.1/6.2 或 Linux 验收。

| 反馈 | 修复与验证 |
| --- | --- |
| 左侧目录居中 | 入口、分类和组件行使用左对齐内容布局，组件条目缩进 12dp；分类使用 bundled Icon。宿主回归覆盖三种宽度及 Default/Dark/Compact。 |
| 正文边缘被裁 | 正文视口覆盖实际 document lane，内容单独保留水平及上下留白；滚动范围按内容高度计算。五种宽度检查四边真实 focus effect 的祖先裁剪及覆盖率，页面身份保留。 |
| 来源文字重叠 | CollapseBody 使用纵向 Flex 与 8dp 间隔，段落随内容宽度换行。三种宽度检查七段来源的顺序、间距和长文本；原生展开图像确认无重叠。 |
| Space.Compact 滚动后中间白线 | 原回归在 1×、微小 translation 后捕获实际提交光栅范围内缺失像素（期望 alpha=0.47、实际为 0）。RoundedEffect 绘制范围向外取整到物理像素，PS 采用精确半开硬裁剪，分区公共端点只缩放一次。2016 个像素点覆盖四种 scale、四种大坐标、七种像素相位及两种 alpha，检查横纵分区的 source-over，无空隙或重复混合。 |

## 实际自动验证

- Windows/MSVC、Ninja Multi-Config，最多 2 jobs。
- `windows-msvc-headless` Debug/Release：相关 targets 构建成功；定向 CTest 各 **6/6** 通过。测试为 `gallery_pages_host`、`gallery_page_viewport`、portable `rounded_effect_gpu`、`rounded_effect_math`、`button_component`、`space_compact`。命令：`ctest --preset windows-msvc-headless-{debug,release} --no-tests=error -R '^rynui\.(gallery_pages_host|gallery_page_viewport|portable\.(rounded_effect_gpu|rounded_effect_math|button_component|space_compact))$' --output-on-failure`。
- `windows-msvc` Release：最终 Gallery 与 embedded shader test 构建成功；RoundedEffect DXIL/SPIR-V 顶点与片元 shader 重新生成。`ctest --preset windows-msvc-release --no-tests=error -R 'rynui\.(embedded_shader|rounded_effect_shader_contract)' --output-on-failure` **2/2** 通过。
- 原生命令：`rynui_token_gallery.exe --page-acceptance --page=space --page=space --page=space --page-width=960 --page-timeout=90 --page-output=out/105-feedback-native/result.json`。D3D12/DXIL、系统字体、当前 1.5× 显示缩放，逻辑窗口 960×900；**3/3 cases、298 checks、52 submitted frames，exit 0**。分别为 Default、Dark、Dark+Compact；每次检查三尺寸滚出后回顶部再滚回、retained 身份、横纵分区像素及入口焦点四边像素，并展开来源查看。检查图像与日志仅保存在 `out/105-feedback-native/`。
- `python scripts/format-code.py --check`：clang-format 22.1.3，**845 文件、0 failures**；OpenSpec doctor healthy，strict validate **105/105**；`git diff --check` 通过。

保留 logical scene v3、packed GPU ABI v2 和 Quad/Glyph/Effect 48/96/112-byte 合同；形状、圆角和既有 AA 宽度不变，真实 ancestor hard clip 保留。

本轮原生结果限于上述 Space 用例与当前 Windows/D3D12 环境；未运行完整 Gallery 矩阵，不声明 Linux/Vulkan、其他实际 DPI/显示器、系统 IME、AT 或 240Hz 性能验收。原生用例没有输入编辑，`host_input_area=false`，不计为任务 6.1 的完整平台通过。原始日志、JSON、BMP 均只留在已忽略的 `out/`。
