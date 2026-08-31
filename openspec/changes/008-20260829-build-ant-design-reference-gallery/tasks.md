## 1. 版本化 Ant Design Reference Catalog

- [x] 1.1 增加锁定 Ant Design `6.5.0`/`740ad964dc2397f33e40944367b0536a7314cc32` 的 source manifest，按 General/Layout/Navigation/Data Entry/Data Display/Feedback/Other 保存 4/7/7/18/20/11/5 共 72 项 stable component identity、名称、source path 与 Introduction/Design Values/Resources 来源；通过 schema、count、order、duplicate 和 source-version tests 验证
- [x] 1.2 增加离线 catalog generator 与 checked-in C++ metadata/hash，生成器不得访问网络且 `--check` 必须拒绝 stale output；使用 `python -B` 并增加 tracked/untracked `__pycache__`、`.pyc`、`.pyo` contract
- [x] 1.3 增加 typed `GallerySupportStatus` overlay，覆盖 `implemented|partial|planned|web-only|deprecated|out-of-scope`、中文摘要、supported/missing scope 与 evidence identifiers；通过全 72 项非空状态、Button/Text/Flex/Space/ConfigProvider 初始 `partial`、implemented evidence gate 和非法枚举 tests 验证
- [x] 1.4 在一个受支持平台记录实际 preset，运行 catalog/generator/status/source contract、相关 unit tests 与 `git diff --check`；以英文 `feat: add versioned Ant Design gallery catalog` 提交并推送本阶段，核对 remote SHA

## 2. Button hover、focus 与 solid visual 核对

- [x] 2.1 增加 Ant Design 6.5.0 Button source contract，锁定 Default outlined 的 normal/hover/active border/text/background、Primary/Danger solid fill、disabled/loading priority、`lineWidthFocus=3`、`outlineOffset=1` 与 `colorPrimaryBorder`；通过直接数值和 token identity tests 拒绝把 hover border 当成 focus ring
- [x] 2.2 修复 Primary/Danger solid Button 的透明 border-box painting，使 fill 覆盖完整 root bounds而不留下 1px 透明边缘；将 Danger hover/active 改为锁定 `colorErrorHover`/`colorErrorActive` palette，通过 1.0/1.25/1.5/2.0 simulated DPI geometry/color tests 验证 HitTest/Layout 不变
- [x] 2.3 扩展 Button 完整状态矩阵：Default hover 只更新现有 1px border/text且 focus effect opacity 为零，Primary/Danger hover 无额外蓝边，pointer focus 无 ring，keyboard focus 显示 1px gap + 3px hollow ring，disabled/loading 优先；通过 component/scene/CPU/GPU reference 和最小 dirty range tests 验证
- [x] 2.4 运行 Button、Theme、Pointer、Focus、RoundedEffect、renderer 与 Token Gallery headless tests 及 `git diff --check`；以英文 `fix: align button state visuals` 提交并推送本阶段，不修改平台真实窗口 evidence，核对 remote SHA

## 3. 非交互 ReferenceSurface

- [x] 3.1 实现 Gallery internal typed `ReferenceSurfaceProps`/content slot 与 generation-checked component record，支持 Theme 驱动的 background/border/radius/status badge/swatch/`LayoutStyle`，不得从 `rynui.hpp` 导出；通过 header/source contract 证明未新增稳定 public API 或通用 Modifier
- [x] 3.2 接入 retained Quad/RoundedEffect/Text scene、intrinsic measure、clip/translation 与 fixed scene fragment，使 reference surface 不注册 InteractionId；通过 nested/empty/CJK/Latin/swatch/shadow、destroy/reuse、Theme update 和 scene order tests 验证
- [x] 3.3 增加非交互语义测试：pointer move/down、Tab、Enter/Space 与 window focus 不得让 reference surface hover/pressed/focused/click，不进入 focus order、不产生 route/capture；普通 Theme/material 更新不得 re-run content 或刷新无关 HitTest
- [x] 3.4 运行 ReferenceSurface、Component lifecycle、Layout、Scene、HitTest/Focus、dependency leak 与 `git diff --check`；以英文 `feat: add non-interactive gallery surfaces` 提交并推送本阶段，核对 remote SHA

## 4. 文档式 Gallery 内容与完整组件总览

- [ ] 4.1 建立稳定 document section model，按 Header/Source、Introduction、Design Values、Foundation/Token、Component Overview、Live Samples 顺序挂载；使用自有简体中文摘要与官方 URL，通过 content/source/copyright-boundary contract 证明不复制整页原文、React/CSS source 或远程图片
- [ ] 4.2 从 catalog 生成七类 72 项 Component Overview，每项显示英文/中文名称、support status、supported/missing scope 与 evidence/source；通过 category heading、entry identity、status filter input 和未实现项无 fake sample tests 验证
- [ ] 4.3 将现有 color/type/spacing/radius/shadow Token 内容迁移为 ReferenceSurface/swatches，确保自定义 palette 不再由只覆盖 normal background 的 Button 模拟；通过 palette hover、shadow layer、focus order 和 retained identity tests 验证
- [ ] 4.4 保留独立 Live Samples 区，只用真实 `ryn::Button`/`Text`/`Flex`/`Space`/`Theme` 展示已支持能力，并明确 unsupported scope；通过 source contract、component count 和 interaction ownership tests 证明目录条目与 live controls 不混淆
- [ ] 4.5 运行 catalog-to-document、content、Theme、Text、Flex/Space、Scene、interaction 和 `git diff --check`；以英文 `feat: build Ant Design reference document` 提交并推送本阶段，核对 remote SHA

## 5. 长文档 viewport、导航与响应式布局

- [ ] 5.1 扩展平台无关 input value 与 SDL adapter，归一化 vertical/horizontal wheel delta并保持 owner-thread、事件顺序、logical coordinate 和 SDL3 隔离；通过 fake SDL-shaped、batch capacity、window focus 与 public forbidden-include tests 验证
- [ ] 5.2 实现 internal `GalleryDocumentViewport` 的 content extent、logical offset、clamp、viewport clip、subtree translation 与 section anchors；通过 top/bottom overscroll、empty/short/long document、resize anchor restore、clip/HitTest 同步和 stale generation tests 验证
- [ ] 5.3 实现 category navigation 与 support status filter controls，anchor jump、wheel scroll 与 current-section 更新只改变必要 translation/material/HitTest；通过 pointer/keyboard navigation、filter combinations、focus order、content closure count 和 sibling isolation tests 验证
- [ ] 5.4 实现宽窗口 navigation+document 双栏与窄窗口顶部 wrap navigation/低列数 content reflow；通过多 viewport、CJK/Latin readability、72 项 reachability、宽窄往返和 stable identity tests 验证不把内容放到不可达区域
- [ ] 5.5 增加 72 项 document benchmark 与 frame diagnostics，锁定 steady-state wheel/navigation 路径容量复用、无全树 remount、无无关 shape/upload、idle 后停止 submit
- [ ] 5.6 运行 platform input、viewport/clip、Layout、HitTest/Focus、Gallery frame、benchmark 与 `git diff --check`；以英文 `feat: add gallery document navigation` 提交并推送本阶段，核对 remote SHA

## 6. 平台通用 Gallery 集成验收

- [ ] 6.1 增加完整 headless Gallery journey，覆盖离线启动、Introduction、七类 72 项、support filter、anchor/wheel、宽窄 reflow、Theme、Button hover/focus、reference surface 非交互和 idle；验证 content/catalog identity 与逐阶段 diagnostics
- [ ] 6.2 增加 Gallery evidence schema，要求 catalog version/commit/hash、category counts、support counts、source links、viewport/section/reachability、Button state、reference interaction count、scene/upload/draw/submit/idle、preset/compiler/platform/driver/shader/font 与截图路径；拒绝交叉平台 identity 和 planning-only implemented 状态
- [ ] 6.3 在一个受支持平台使用正式 preset 运行 catalog/generator、全部平台通用 unit/headless/contract/benchmark、public dependency、lock/license、无网络 runtime 和 Python cache 检查，记录实际 OS/compiler/preset/result
- [ ] 6.4 运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive` 与 `git diff --check`；以英文 `test: validate reference gallery contracts` 提交并推送平台通用 evidence，不要求另一平台重复本组合同，核对 remote SHA

## 7. Windows 专属验收

- [ ] 7.1 使用 `windows-msvc` preset clean configure，完成受 Windows 分支影响的 Debug/Release build 与 CTest，核对 Ninja Multi-Config、MSVC x64、BUNDLED 依赖、Win32 wheel/input、DirectWrite system font discovery 和 D3D12/DXIL 来源，保存独立 Windows 结果
- [ ] 7.2 在 Windows/MSVC/D3D12/DXIL 真实窗口以系统 display scale 和 1.0/1.25/1.5/2.0 acceptance render scale 浏览 Introduction、Foundation 与七类末尾，操作 category/filter/wheel/keyboard、Default/Primary/Danger hover/active/focus 并正常退出；保存截图、driver、font、scale、退出码与 diagnostics
- [ ] 7.3 人工核对非交互目录无蓝色 hover/focus、Default 只有既有 1px hover border、solid Button 无透明边缘/蓝色外圈、keyboard focus 为 1px gap + 3px ring、CJK/Latin 可读、72 项全部可到达；运行 Windows evidence passed contract、dependency/shader/lock/license 与 cache 检查
- [ ] 7.4 运行 Windows 受影响平台测试、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Windows reference gallery` 提交并推送 Windows evidence，核对 remote SHA，不修改 Linux 清单

## 8. Linux 专属验收

- [ ] 8.1 使用 `linux-gcc` clean configure 和 `linux-clang` Debug build，完成受 Linux 分支影响的 CTest，核对 Ninja Multi-Config、标准 C++20、BUNDLED Wayland/libdecor、Fontconfig system font 与 Vulkan/SPIR-V 来源，保存独立 Linux 结果
- [ ] 8.2 在原生 Linux Wayland/GCC/Vulkan/SPIR-V 真实窗口以至少两档实际 display scale 浏览完整文档，操作 category/filter/wheel/keyboard、Button hover/active/focus 与正常退出；保存截图、window system、driver、font、scale、退出码和 diagnostics，不以 X11 代替
- [ ] 8.3 人工核对非交互目录、Button state、CJK/Latin、clip/scroll、宽窄 reflow 与 72 项 reachability；运行 Linux evidence passed contract、dependency/shader/lock/license、Fontconfig 与 cache 检查
- [ ] 8.4 运行 Linux 受影响平台测试、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Linux reference gallery` 提交并推送 Linux evidence，核对 remote SHA，不修改 Windows 清单

## 9. Change 收口

- [ ] 9.1 在准备 archive 时运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、最终受影响 CTest、`git diff --check`、remote SHA 与 clean worktree 检查；确认平台通用、Windows 与 Linux checkbox/evidence 各自真实完成，README、AGENTS、architecture、generated token docs 与 OpenSpec 职责未混写，本项不替代任何平台验收
