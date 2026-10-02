# 代码格式

RynUI 自有 C++ 和 HLSL 使用根目录 `.clang-format`。统一使用 clang-format **22.x**；本次格式基线使用 22.1.3。脚本需要 Python 3.9 或更高版本、Git 和 clang-format，不在仓库安装工具依赖。

## 约定

- 行宽 120，四空格缩进，不使用 Tab；大括号放在声明或控制语句所在行。
- `if`、`else`、`for`、`while`、`do` 的语句体始终有大括号；保留 `else if` 链。
- 每条声明只定义一个变量，包括局部变量、成员和全局变量。函数参数、模板参数和结构化绑定不属于多变量声明。
- 相邻 struct、class、enum 和函数定义用一个空行分隔；普通函数体展开，空函数可保持单行。
- 保留 include 和 using 的顺序；稳定组件、逻辑场景、shader ABI 和运行行为不因格式化而改变。

`InsertBraces` 与 `SeparateDefinitionBlocks` 负责大括号和定义间空行，详见 [LLVM 官方选项说明](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)。clang-format 不负责把多变量声明拆成独立声明，这部分需要在代码修改和 review 时检查。宏定义或跨预处理指令的控制流需要人工检查大括号；不能仅用 formatter 通过代替编译验证。

## 格式化与检查

在仓库根目录运行：

```text
python scripts/format-code.py
python scripts/format-code.py --check
```

脚本只处理 Git 跟踪的 `include/`、`src/`、`examples/`、`tests/`、`shaders/` 中的 C/C++ / HLSL 文件，排除 `tests/fixtures/`。`out/` 中的第三方代码、生成 `.inc`、构建输出不在处理范围。`--check` 只检查格式，有差异时返回非零状态。

若 clang-format 未在 PATH 中，使用 `--clang-format` 指定本机工具路径。例如 Visual Studio 安装包含 LLVM 时，在 PowerShell 中运行：

```powershell
$formatter = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\clang-format.exe'
python scripts/format-code.py --clang-format $formatter
python scripts/format-code.py --check --clang-format $formatter
```

Visual Studio 版本、edition 与安装目录因机器而异，应使用实际路径。VS Code 的 C/C++ 扩展读取 `file` 样式，并显示 120 列参考线；若扩展内置 formatter 版本不同，可在个人设置中指定 `C_Cpp.clang_format_path` 指向 22.x。

修改 `rounded_effect.hlsl` 后需要同步 `rounded_effect.lock.yaml` 的 LF 归一化 SHA256，并重新生成、验证 shader。格式变更完成后运行 `git diff --check` 及受影响 preset 的 build / CTest。

## 2026-10-02 格式基线验证

本次使用 clang-format 22.1.3 整理 389 个自有源文件，并在 73 个文件中拆分多变量声明。格式检查通过，C++ / HLSL 中没有超过 120 字符的行；控制语句体和多变量声明的词法扫描没有剩余候选。批量 formatter 的词法核对，以及声明拆分后的类型、初值和顺序核对均通过。

- `windows-msvc-headless`：Debug / Release 构建通过，CTest 各 36/36 通过。
- `windows-msvc`：Debug / Release 干净构建通过，CTest 各 242/243。
- 三种 shader 的 DXIL / SPIR-V 编译和现有 shader 合同通过；rounded-effect 源码锁定哈希已同步。
- OpenSpec doctor 健康，strict validate 39/39 通过；`git diff --check` 通过。

原生两种 configuration 的唯一剩余失败是 `rynui.gallery_document_model` 的 `Gallery partial/planned support overlay drifted` 断言。已从格式修改前的提交 `1ccc010` 导出完整源码，以相同 MSVC preset 单独重建并运行该测试，复现相同失败；此次没有调整原有状态计数断言。

增量 Debug 构建一度出现 `pointer_allocation` 与 `focus_lifecycle` 崩溃。原始源码新构建的两项测试正常；按 [开发构建说明](building.md) 在 UTF-8 控制台重新 configure 并干净重建后，两项测试通过。需要保留 configure / build 的控制台编码一致，以及正确的 MSVC 头文件依赖信息。
