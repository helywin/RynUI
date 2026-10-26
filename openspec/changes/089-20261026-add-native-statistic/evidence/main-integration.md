# Statistic main 集成验收

2026-10-26：main `528eed92ae66977be64cba8f5d191e90824aff47` 与独立 Astra R7 已通过的 Statistic worker `9e96805f9fecbce3baaaebf1364724338bffcc45` 显式合并。R7 在约定首版范围未发现可达 P1/P2，worker existing-binary 的各 15/15 不替代本记录的 main 新构建。

## 合并保留与能力

并行 Theme、窗口 host、CMake 追加冲突保留双方接入：现有 Avatar、Rate 在前，Statistic 在后。Statistic 自有 API/model/host/Theme resolver、四个测试和文档，以及共享 Skeleton 生产修复与 integration tests，均与已审 worker blob 一致。父 Avatar/Rate 组件、Gallery、document-hit 和 retained conversion 源码保持，未导入 AutoComplete 或 Tree WIP。父只补中英文 README、组件进度及此验收记录。

首版具备真实 typed 整数/浮点/String、截断补零、分组和自定义 formatter、四类 retained 富 slots 与 Skeleton loading。R1–R6 的正负零、初始挂载回调销毁、私有子结构退役、挂载期间最新 Prop、最终输入边界、局部捕获异常回滚均由原断言保留；回滚仅退役捕获的完整 generation，保留原异常与回调新建组件。

## main 实际命令

Windows VS MSVC x64、`windows-msvc-headless`、Ninja Multi-Config、BUNDLED，最多 2 jobs。VSDevShell 环境执行：

```text
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_public_api_tests rynui_statistic_component_tests rynui_skeleton_component_tests rynui_skeleton_integration_tests rynui_rate_model_tests rynui_rate_theme_tests rynui_rate_public_api_tests rynui_rate_component_tests rynui_rate_lifecycle_tests rynui_rate_font_limit_tests rynui_rate_focus_order_tests rynui_rate_review_tests rynui_avatar_theme_tests rynui_avatar_component_tests rynui_portable_theme_algorithm rynui_portable_theme_runtime rynui_popover_integration_tests rynui_timeline_review_tests rynui_notice_review_tests
out/build/windows-msvc-headless/tests/Debug/rynui_avatar_theme_tests.exe out/statistic-main-goldens-debug
ctest --preset windows-msvc-headless-debug --parallel 2 --output-on-failure -R "^rynui\.(statistic_.*|skeleton_component|skeleton_integration_(editor|select|retirement|boundaries)|rate_.*|avatar_.*|portable\.theme_(algorithm|runtime)|popover_(wheel|children|covered|clips)|timeline_review_(fill|document_hit)|notice_review_clip_contract)$"
```

Release 使用相同 targets/filter，preset 改为 `windows-msvc-headless-release`，金样程序使用 Release 并输出 `out/statistic-main-goldens-release`。上述 Avatar Theme 测试程序序列化完整当前 Theme，包含实际组合后的 Statistic。

全部 exit 0：配置 **6.9996425 秒**；Debug 实际编译/链接 **221.612212 秒**，CTest **45/45、real 9.64 秒**；Release 实际编译/链接 **237.6152181 秒**，CTest **45/45、real 4.88 秒**。集合为 Statistic 11 项、Skeleton 5 项、Avatar/Rate 20 项、共享 Theme 2 项、Popover 4 项、Timeline 2 项与 notice 1 项。无历史 full suite 或 Gallery scoped flags 重跑。

日志为 `out/089-main-{debug,release}-{build,test,goldens}.log`、对应 CTest XML、`out/089-main-debug-configure.log`、`out/089-main-doctor.json`、`out/089-main-strict.log`；忽略目录的 `out/089-main-msvc.ps1` 只提供环境与记录。保留既有 MSVC warning 及 VSDevShell 的 `vswhere.exe` 提示。

## 金样与基础检查

```text
python openspec/changes/089-20261026-add-native-statistic/evidence/verify-main-theme.py 528eed92ae66977be64cba8f5d191e90824aff47 9e96805f9fecbce3baaaebf1364724338bffcc45 out/statistic-main-goldens-debug
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

五份 fresh Debug/Release 金样字节一致，并替换 canonical JSON。双基线深比较保留 main **16,802**、worker **16,667** 个旧 typed leaves（只排除顶层 identity）；保留父 **38** 个 IIFE 原文/顺序，追加 Statistic **1** 个；保留父 **200** 个 TokenIdentity ordinal，仅追加 **2** 个。12 个实际 main Statistic/Skeleton x64 PE 均为默认 **1,048,576 bytes** 栈。

clang-format 22.1.3 检查 **761 files、0 failures**；显式 OpenSpec **1.14.0** doctor healthy、strict **92/92**；工作树与 staged diff check 通过。未升级全局工具、扩大栈或改变 renderer ABI。

## 范围

仅证明上述源码集成与本机 HEADLESS 合同。Timer/Countdown/Countup、locale、动画为首版排除；Statistic 交互 Gallery、完整 Gallery/native startup、组件 GPU/人工视觉、Linux、系统 IME/AT 尚未验收。093 的三个 Core 依赖在本次 main 发布后才可记录冻结基线并启动 Gallery。只推 main，不推 `t3/*`，没有 PR 或 archive。
