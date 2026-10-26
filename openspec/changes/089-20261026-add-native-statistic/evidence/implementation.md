# Statistic 实现阶段证据

工作树：`C:/Users/jiang/.t3/worktrees/RynUI/t3-6f192459`，分支 `t3/add-native-statistic`，任务基线 `8056de67953ee73015c7559f1782033d85e08573`。仅本地实施，无远端操作。

## 锁定合同

上游路径 `D:/code/RynUI/out/upstream-ant-design-6.6.5`，实际 `git rev-parse HEAD` 为 `4a39f54842eade4e565ab336ef6097cd7e723cdd`。已读取 Statistic.tsx、Number.tsx、utils.ts、style/index.ts、index.en-US.md、index.test.tsx 与其 snapshot，以及 genFontSizes/genFontMapToken/genSizeMapToken/Compact size 公式。负 precision 截断为整数，formatter 优先，非十进制字符串原样；内容字号按 genFontSizes 的 heading3 偶数规则（Default 24 / Compact 20），marginXXS 使用 sizeUnit*(sizeStep-3)。不移植 DOM/CSS。

## 实现

- `StatisticValue` 保存 int64/uint64/double/String，使用 to_chars 最短浮点表示，不经过 to_string 六位截断。String 直接十进制分组/截断/补零。非有限 typed double 与 precision>100 拒绝；formatter 在候选 model 上成功后发布，回调返回后核对完整 ComponentId generation。
- title/value/prefix/suffix 独立 typed slots，只挂载一次；普通文本由 Signal 驱动。Skeleton 复用已存在的 retained loading gate，标题保留，隐藏根退出 flow。内部裁 content、外层裁 decoration，沿用 document/window-layer 输入边界。
- WindowComponentServices 将 Statistic 追加到现有 Timeline document-clipped fill roots，复用共同 logical effect 填充路径；没有 GPU ABI、shader 或专用上传路径修改。
- Theme config/equality/派生/继承/独立 seed/hash/JSON/订阅/dirty 完整接入；新 token 用 shared ownership。未更改默认 PE 栈设置。

## Debug 已验证

本机 Windows，MSVC 19.51，`windows-msvc-headless`，Ninja Multi-Config，BUNDLED 显式依赖模式；FreeType/HarfBuzz/utf8proc/SheenBidi source 和 4 份验证字体复用本地锁定缓存，无下载。构建最多 2 jobs。

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_portable_theme_algorithm rynui_timeline_review_tests
ctest --preset windows-msvc-headless-debug -R "rynui\.(statistic_|portable.theme_algorithm|timeline_review_(fill|document_hit))" --output-on-failure
```

13/13 通过（1.93 秒）：Statistic 10 项、Theme algorithm 1 项、父 Timeline fill/document_hit 2 项。实际验证 uint64/int64 极值、超过六位的小数、分隔符、formatter、非法 finite 更新恢复；Theme 订阅最小 dirty、独立 seed、继承、最终字体下限与祖先原子 preflight；真实 R8 glyph coverage、quad 源和实际 clipped fill/effect coverage、region/document hit clip；loading 不绘制富编辑器 glyph，hidden 无 draw/flow/gap；普通属性/Theme/隐藏/零尺寸恢复保留编辑草稿、session/ref/身份；同步 blur 卸载及同 ref 重绑、formatter 卸载、owned block 清理、controlled Select 拒绝关闭恢复、Modal/Spin gate、不空转 deadline。公开 docs/statistic.md 示例实际挂载执行，另有文本一致性检查。

```text
out/build/windows-msvc-headless/tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens
python openspec/changes/089-20261026-add-native-statistic/evidence/compare-theme.py
```

实际重新生成 5 份 goldens；深比较旧 typed JSON 所有 16,587 叶值保持，36 个旧 IIFE 原文/顺序保持，196 个旧 TokenIdentity 顺序保持，仅新增 statistic 与 identity 变化。clang-format 22.1.3 全仓检查 736 份自有源文件通过（0 failures）。Release 与最终收口证据在后续 validation.md 记录；此阶段不声明其已通过。

## 覆盖边界

未运行历史 full suite、Gallery 长测、原生窗口、GPU、Linux、系统 IME/AT；不将 HEADLESS 证据扩大到这些环境。首版仅 Statistic，不包含 Timer/Countdown/Countup/动画/locale 国际化。未修改 README、docs/component-completion.md、Gallery 或 support-overlay/generated catalog。父负责 Astra 审查及 main 发布，worker 不创建 children、PR 或 archive。
