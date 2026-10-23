# 平台通用验证

## 环境与范围

- 基线：`df43421264731bae5c43939fdeaaddbcc3b9e5fb`；隔离工作树 `C:/Users/jiang/.t3/worktrees/RynUI/t3-e1b1d110`，本地分支 `t3/native-input-number`。
- Windows x64，MSVC 19.51.36256，`windows-msvc-headless`，Ninja Multi-Config；独立构建目录为本工作树 `out/build/windows-msvc-headless`。最大 4 编译作业。
- 本机从 `D:/code/RynUI/out/typography-dev.ps1` 仅复制 helper 到自己的 `out/input-number-dev.ps1`，将并发改为 4；helper 不提交，不共享构建产物。
- Ant Design 6.6.5 checkout HEAD 已核实为 `4a39f54842eade4e565ab336ef6097cd7e723cdd`，读取 input-number 组件、API 文档、style/index.ts 和 style/token.ts；不改变锁定源码和 source lock。

## 结果

- Debug：聚焦 CTest **13/13** 通过；原始 CTest 日志见 `debug-ctest.log`。
- Release：聚焦 CTest **13/13** 通过；原始 CTest 日志见 `release-ctest.log`。
- clang-format 22.1.3：`scripts/format-code.py --check` 最终检查 **531** 个自有源文件，**0** 失败。
- `openspec doctor --json` healthy，`openspec validate --all --strict --no-interactive` **65/65**，`git diff --check` 通过。
- 五份 Theme golden 的旧 JSON 字段逐份与模型阶段 HEAD 比较，移除 `identity` 和新增 `inputNumber` 后完全一致；新字段颜色 7 项、metrics 3 项，包含 Default、Dark、Compact 及两种组合顺序。

## 命令

在 MSVC x64 Developer Environment 中，先 `cmake --preset windows-msvc-headless`。Debug 和 Release 分别使用对应 build/test preset。实际 helper 生成的构建等价于：

```powershell
$numberTargets = @(
    'rynui_input_number_model_tests', 'rynui_input_number_public_api_tests',
    'rynui_input_number_component_tests', 'rynui_input_number_theme_tests',
    'rynui_portable_theme_algorithm', 'rynui_portable_theme_runtime',
    'rynui_portable_input_component', 'rynui_portable_input_variant',
    'rynui_portable_input_actions', 'rynui_portable_text_input_session',
    'rynui_portable_password_component', 'rynui_portable_search_component'
)
cmake --build --preset windows-msvc-headless-debug --parallel 4 --target $numberTargets
cmake --build --preset windows-msvc-headless-release --parallel 4 --target $numberTargets
$numberFilter = '^rynui.(input_number_(model|public_api|component|theme)|portable\.(theme_algorithm|theme_runtime|input_component|input_variant|input_actions|text_input_session|password_component|search_component|backend_boundaries))$'
ctest --preset windows-msvc-headless-debug --parallel 4 -R $numberFilter
ctest --preset windows-msvc-headless-release --parallel 4 -R $numberFilter
```

Theme golden 通过 `rynui_portable_theme_algorithm --write-goldens` 生成，额外用 Python JSON 比较保留所有旧字段。没有运行完整 CTest，也没有用其他工作树的二进制代替验证。

## 覆盖与边界

数值模型覆盖十进制 0.1 连加、0.2+0.1、负数、空值起步、safe integer 边界、半值舍入（含 1.005）、precision 与向内边界量化、无可表示值的范围以及非法值/配置原子性。

组件测试通过真实 FocusManager/PointerRouter、TextCommitted 和 composition 注入覆盖：受控拒绝与同步回写、尾零草稿保留、空/partial/非法文本、Enter/blur 规范化、按钮与 Up/Down、上下限、keyboard/controls 开关、disabled/readOnly、滚轮不改值、preedit 屏蔽、有效/无效/同值 commit、Escape 取消、retained identity/slots、ref/session/capture 清理、迟到事件拒绝、change/blur 同步销毁及 focus callback 重定向。Theme 测试覆盖继承、reset、Dark/Compact、独立 component algorithm、颜色不测量和 metrics 局部更新。

以上是平台通用 headless 合同。未运行真实 GPU 窗口、系统 IME、Linux、人工视觉或原生 DPI 验收。首版语法为 ASCII 普通小数，安全整数范围内最多 12 位小数；不提供 stringMode、任意精度、formatter/parser、wheel stepping、spinner、长按连发或 hover reveal 动画。按钮复用 Input affix 布局，表面继承 Input tokens。

父任务负责 Gallery、支持矩阵、README、已知 Alert/Popconfirm P2、跨组件集成和最终平台验收。本工作树只创建本地英文 Conventional Commits；没有 push、PR 或 archive。

## Astra R1 补充

在 `91f1019` 后复核两项数值/IME P2，失败复现、修复策略、三条取消路径与受影响 Debug/Release 合同见 [R1 专项证据](astra-r1/verification.md)。本节以上保留首次实现的验收记录。

## 独立 main 集成第一阶段（2026-10-23）

工作树 `C:/Users/jiang/.t3/worktrees/RynUI/t3-de868eb7`，本地分支 `t3/integrate-menu-input-number`，从 `69689046bc9a99b94c98086cb3c849b79015acbc` 先合并 Menu/Dropdown `4fa26a9`（本地合并提交 `37c6dda`），再合并本变更 `8cea30f3738c8f79e4ace10702c7c11b5888a991`。

- 新增 `spin_preserves_numeric_input_gates`：通过实际 Spin loading signal、PointerRouter、FocusManager、InputRef 与 text session，验证 loading 取消 preedit/session、阻止真实数值按钮与 Up/Down、拒绝 ref focus/select 及迟到 IME；退出 loading 保留自身 disabled/readOnly；恢复后新 preedit 仍抑制步进，取消后按钮和键盘可用；数值、editor/action/text identity 与单次 slot mount 保留。
- Debug 29/29 聚焦 CTest 通过（10.35 秒），含四个 InputNumber、Menu/Dropdown、Input/editor/session、Theme、Modal/Drawer/Popconfirm、Tag/Badge、Alert/Spin/Progress 和 backend boundaries。
- 五份 golden 由本工作树最新 Debug Theme executable 重新生成；逐份与 `6968904` 的 JSON 比较，27 个旧顶层字段（不含 `identity`）的完整嵌套内容保持一致；仅新增 `menu`、`dropdown`、`inputNumber`。
- clang-format 22.1.3 检查 549 个自有源文件，0 failures。保留已有 ThemeScope 引用提交、reference_cell 默认主题栈占用修正、popup child-layer 排序、blocked subtree 输入与全部参与者生命周期。

父任务随后授权合并已提交 main `b27651db3b8e2b6ca1edae01b722390819385e24`；最终 Table/Pagination 共同集成结果另行记录。本阶段仅本地提交，不重复完整 117/119 套件、不重复 Astra 评审，也不改 Gallery/全局支持文档。
