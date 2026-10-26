# Design

## Context

见 proposal.md。基线 main `0f578dde`：`cmake/RynUIShaders.cmake` 将 shader 编译到 generated/shaders，再逐目标复制至 exe/shaders；两个 SDL renderer 各自使用 ifstream 读文件。普通 text/button/layout/Gallery 入口指定 exe/fonts 的验收字体 fallback，examples/CMakeLists.txt 复制这些字体。平台默认字体链已支持显式文件在前、DirectWrite/Fontconfig 系统链在后，FontRuntime 也已有 load_font_bytes；不重复实现字体发现或内存字体加载。

## Goals / Non-Goals

**Goals:** 通用轻量只读资源包、可复用 CMake 入口、正确增量构建、内置 shader 与不依赖旁置资源的原生示例。

**Non-Goals:** 不依赖 Qt，不提供完整 RCC、动态挂载、压缩、加密、热更新或新的字体引擎。qrc 仅支持明确记录的 prefix/alias/file 子集，不支持的语义直接拒绝。

## Decisions

- **借用 qrc 清单语义，显式资源包访问。** 使用 RCC/qresource/file 的 prefix/alias 描述，生成持有静态 byte arrays 与路径表的具名 C++ accessor。公开 ResourceBundle 提供不可变 byte span 和可区分缺失/空文件的查找。相比全局自动注册，不引入初始化顺序、锁或跨模块卸载状态；实际 accessor 引用也解决静态库 dead stripping。路径为大小写敏感的 `:/prefix/alias`，alias 使用正斜线，拒绝 parent traversal、冲突和非法段；普通文件内容不作文本假设。
- **构建时生成，运行时无工具依赖。** 新 CMake helper 接入 target 和资源清单，使用已有构建 Python 生成 C++/header 和 depfile。兼容 CMake 3.25 与 Ninja Multi-Config，显式依赖生成的 shader 输入；清单与资源变化正确触发生成，内容未变不重写源码。相比运行时读目录，不受 cwd 和发布布局影响。基础层只含标准 C++，不链接平台或 renderer。
- **shader 包由 SDL renderer 拥有。** 将现有实际编译的 12 个二进制输入组成内置包，保留原 HLSL 编译与 reflection 合同，不在资源层编译 shader。两个 renderer 从 byte span 创建 shader，移除默认磁盘目录参数/读取和各示例旁置 shader 部署；不改变逻辑 scene、GPU ABI 或上传所有权。
- **系统字体复用现有实现。** 普通示例采用没有旁置 fallback 路径的默认 request，不再把 validation 字体当发布资产。应用显式字体配置、平台系统 fallback 及测试显式锁定字体保持；不写死 Windows 字体文件，也不将系统字体随 exe 分发。必要时资源包 bytes 可直接交给既有 FontRuntime::load_font_bytes，不新增平行字体 API。系统缺少所需文字时保留明确诊断。
- **先通用合同、再实际部署。** Windows MSVC `windows-msvc-headless` Debug/Release 覆盖资源与构建合同；原生 `windows-msvc-debug/release` 实际新构建，用复制出的 exe、无 shaders/fonts 的目录和不同 cwd 验证。原生 Linux/Vulkan/Fontconfig 使用独立平台任务，不能由 Windows 结果勾选。

## Risks / Trade-offs

- [静态库资源被裁剪] → 显式 accessor 引用，实际静态库链接与读取测试。
- [新增或修改资源没有重新生成] → 清单/script/生成输入的显式依赖与 depfile，实际增量构建及 no-op 检查，不只检查脚本文本。
- [生成路径包含空格/Unicode 或 Multi-Config 冲突] → 参数逐项传递、稳定独立产物路径，MSVC Ninja Multi-Config fixture 验证。
- [内嵌内容增加可执行文件体积] → 只打包运行所需二进制 shader，反射 JSON 和验收字体不作为默认运行资源；不为当前规模引入压缩或自制 archive。
- [系统字体配置差异] → 保留当前发现/coverage/替代字形诊断，记录实际 system families；不以固定验收字体冒充默认系统链。

## Migration Plan

先提交规划，再实现可独立验证资源基础层，再接通 shader/示例默认字体，最后实际独立目录启动与全新 Astra 审查。组件并行任务不由此变更修改；合并其后续提交时保留资源构建和默认入口。每个通过阶段英文 Conventional Commit 后只推 main 并核对远端 SHA，不创建 PR/archive。

## References

通过 Context7 核对 [Qt resource system](https://doc.qt.io/qt-6/resources.html) 与 [qt_add_resources](https://doc.qt.io/qt-6/qt-add-resources.html) 的 prefix/alias、静态库初始化问题；只采用合同思想。[CMake custom command](https://cmake.org/cmake/help/latest/command/add_custom_command.html) 确认 Ninja Multi-Config 自 3.17 支持 DEPFILE，实施限制在项目最低 CMake 3.25 可用能力内。
