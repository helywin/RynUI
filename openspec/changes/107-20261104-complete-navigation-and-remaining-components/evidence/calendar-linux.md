# Calendar Linux 验收

- 平台：Linux，GCC，`linux-gcc-headless` / Ninja Multi-Config，锁定 BUNDLED 源缓存；分别运行 Debug 与 Release。
- 构建：`cmake --preset linux-gcc-headless`；`cmake --build --preset linux-gcc-headless-{debug,release} --target rynui_calendar_tests rynui_calendar_gallery_tests rynui_theme_algorithm_tests rynui_theme_runtime_tests rynui_portable_theme_public_api -j2`。
- CTest：`ctest --preset linux-gcc-headless-{debug,release} -R '^rynui.(calendar_|theme_algorithm$|theme_runtime$|portable.(theme_public_api|backend_boundaries)$|gallery_sample_source_generated$)' --output-on-failure`；最终每个配置 **16/16 通过，0 失败**。包含 9 个 Calendar 合同、独立 Gallery、Theme runtime/algorithm、完整 golden/ordinal 基线、公开 Theme/Core 边界及真实样例源码生成检查。
- 覆盖：公历/闰日/年边界/locale week year、受控拒绝与事件顺序、规则变化、重叠日期保留真实编辑草稿、mini/fullscreen/RTL/零尺寸、Core preedit/嵌套 Popover、header 响应替换与工厂回滚、键盘/deferred focus、stale/跨线程 ref、销毁/异常清理，以及纯颜色 Theme 更新不扩大布局或重建无关组件。
- Gallery：普通日程与自定义 header/fullCell 两组实际原生样例，真实年月/模式/Today/选择和日程回调；生成器统计从基线 91 增至 **93**，既有 **458** 个冻结测试 ID 保留。catalog `--check --self-test` 通过。
- 五份完整 Theme golden 保留原 **62** 个顶层字段（identity 除外），新增独立 Calendar 域；既有 TokenIdentity 名称及顺序不变。clang-format 22.1.3、OpenSpec doctor、全部 strict validate 与 `git diff --check` 通过。
- 边界：以上是 HEADLESS/CPU 逻辑场景与输入服务证据；未运行本组件 GPU/原生窗口、Windows/MSVC、实际桌面或系统 IME/OS 辅助技术验收。63.3、64.3、65.1 保持父线程范围。原始构建/测试日志及上游源码副本仅保存在忽略的 `out/calendar-carousel/`。

- Carousel 组合收口后，Debug/Release 各 **35/35** 定向检查再次通过（含全部 9 个 Calendar 合同及其 Gallery）。补齐 Calendar 域参与 ThemeSnapshot 相等性，并断言实际颜色更新而非仅不重建；五份完整 golden 现为 64 字段，原 62 字段保护不变。详情见同目录 `carousel-linux.md`。
