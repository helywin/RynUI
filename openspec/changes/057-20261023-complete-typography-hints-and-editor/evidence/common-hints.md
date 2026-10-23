# Typography 提示：平台通用验证

2026-10-23，Windows，MSVC 19.51.36256 x64，Ninja Multi-Config，BUNDLED 依赖，`windows-msvc-headless`，HEADLESS/Recording；未使用原生 GPU 结果代替共同合同。

- Debug 构建通过。完整 CTest 首轮 98/99，唯一失败是本次新增 hover 测试未在开始计时后再推进 100ms。修正测试时钟后，`ctest --preset windows-msvc-headless-debug -R typography_interaction --output-on-failure` 1/1 通过（2.02s），其他 98 项均已有本次成功结果；没有把首轮报告写为 99/99。完整首轮 230.08s，input_scene_allocation 通过（222.72s）。
- Release 构建与完整 CTest 99/99 通过（10.32s），包括修改后的 Typography 测试、Tooltip、输入、布局、分配、后端边界和 bidi。
- 新回归验证 hover/focus、普通/成功/自定义/空提示、失败复制不报成功、反馈到期、编辑与禁用关闭、实际截断和 resize/展开、兄弟 Link、identity、失活/销毁与 deadline 清理。
- clang-format 22.1.3 全仓检查 459 个自有源文件，0 failures；OpenSpec 1.14.0 doctor healthy、strict 57/57、`git diff --check` 通过。
- 原生日志接入按 focusable 查找 Typography action，避免 Tooltip 的非 Tab wrapper/popup 改变既有动作索引。

本阶段不证明 Windows 原生提示像素或人工系统 IME，也不证明 Linux；两平台验收独立在 tasks 中跟踪。
