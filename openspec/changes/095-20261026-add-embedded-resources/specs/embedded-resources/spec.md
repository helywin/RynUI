# Spec Delta

## Purpose

提供可随 C++ 程序或静态库链接的只读资源包，使应用通过稳定路径获取内嵌原始内容，不依赖启动目录或旁置资源文件；首先用于 RynUI 原生 shader 和脱离开发目录运行的示例，并保留系统字体作为默认字体链。

## ADDED Requirements

### Requirement: 只读路径资源

资源包 SHALL 使用带 prefix/alias 的稳定资源路径提供原始 bytes，不依赖 cwd、exe 路径或运行时文件读取。空文件与不存在路径 SHALL 可以区分，读取 SHALL 不修改或复制整个资源内容。资源内容在所属模块存活期间 SHALL 有效。

#### Scenario: 独立目录读取二进制和空文件
- **WHEN** 程序从任意 cwd 读取包含零字节的内嵌二进制、空文件与不存在的 alias
- **THEN** 已存在内容逐字节正确，空文件被识别为存在，不存在路径明确返回缺失

### Requirement: 可重复构建与静态库链接

构建工具 SHALL 从资源清单生成可链接的 C++ 资源，支持路径 prefix 和 alias，拒绝重复 alias、非法资源路径和缺失输入。资源输入或清单变化 SHALL 触发更新；未变化内容 SHALL 不产生无意义重编译。静态库中的已引用资源 SHALL 不因链接裁剪而消失。

#### Scenario: 修改原始资源后重新构建
- **WHEN** 使用 Ninja Multi-Config 构建带空格路径的资源包，修改一个资源后再构建
- **THEN** 程序读取新内容，未修改构建保持稳定，Debug/Release 均能引用静态库内资源

### Requirement: 内置 shader 启动

SDL renderer SHALL 从内嵌资源读取自身需要的 shader，根据设备支持选择 DXIL 或 SPIR-V。Quad、Glyph 与 RoundedEffect 的 vertex/fragment SHALL 均可用，运行时 SHALL 不依赖 shader 源文件、编译器或 exe 旁的 shaders 目录。

#### Scenario: exe 离开 shaders 目录
- **WHEN** 将示例 exe 复制到不含 shaders 的独立目录并从另一 cwd 启动
- **THEN** renderer 能创建实际 GPU pipeline 并正常绘制，资源来源确认为内嵌

### Requirement: 默认系统字体链

普通原生示例 SHALL 默认使用平台系统字体链，不要求 exe 旁存在 fonts 或固定验收字体。应用显式字体 SHALL 仍位于系统 fallback 前；显式测试/应用 fallback 配置 SHALL 保留。缺失实际所需字体 coverage SHALL 有明确诊断，不伪造系统提供的字形。

#### Scenario: 无旁置字体启动
- **WHEN** 不提供自定义字体，将示例 exe 放入无 fonts 的目录并在具有所需字形的系统上启动
- **THEN** 使用系统字体完成实际文字布局/绘制，telemetry 标明系统字体，显式锁定字体测试不受影响

### Requirement: 层次与部署证据

资源基础层 SHALL 不依赖 Qt、SDL、renderer 或系统字体服务；renderer 和应用拥有各自资源包。发布验收 SHALL 明确实际构建配置、原生 backend、独立目录和字体来源，平台通用验证不得代替未运行平台的原生验收。

#### Scenario: HEADLESS 与原生部署分别验证
- **WHEN** 构建资源单元测试及原生示例并执行部署检查
- **THEN** HEADLESS 保持平台隔离，实际原生启动结果单独记录，未运行的 Linux/GPU/字体环境保持待验
