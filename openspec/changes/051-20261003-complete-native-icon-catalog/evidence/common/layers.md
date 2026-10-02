# 内置 Icon 层与 motion 合同

2026-10-03，Windows 11 / MSVC 18 / Ninja Multi-Config，平台通用合同。

- `windows-msvc-headless` Debug/Release：Icon component/catalog、Text component/frame/scene、Theme runtime、Tooltip、Input/Password/Search、Typography interaction、Button，共 12/12；耗时 7.21 / 5.00 秒。
- 新 `icon_component_tests.cpp` 验证 2→4→1 layer 源切换、同 Component/root 与共同 layer 前缀身份、物理 range compaction 与独立绘制顺序；显式/派生双色 alpha、Theme primary/语义 tone、字号和 1.5 density 更新。
- 颜色/角度更新不增加 shape、measure、atlas dirty coverage，不重新执行 Content 或 shaping 后续 sibling；隐藏清空全部 layer、非法源/角度保留有效状态，失败挂载无 scene 泄漏。
- 实际 AnimationRuntime clock 验证两圈线性 spin、静态角度叠加、reduced motion、motion=false、hidden、窗口失活、Tooltip title branch 关闭与销毁 scope/target 清理；静止状态无 deadline。
- clang-format 22.1.3 与 `scripts/format-code.py --check`：423 个自有文件通过；OpenSpec doctor healthy、strict 51/51、diff check 通过。

本阶段未将自定义向量、Gallery 或真实 GPU 窗口项标为完成；它们依照 tasks 的后续阶段验收。原始执行日志位于开发机 `out/icon-layer-common-{debug,release}.log`、`out/icon-layer-{doctor,openspec}.log`。
