# Spec Delta

## Purpose

使原生示例可以离开开发时的资源目录运行：Windows 通过标准 rc 资源嵌入必要 shader，其他平台保留同等只读资源能力，普通示例默认使用系统字体 fallback，并通过实际构建和独立目录启动验证结果。

## ADDED Requirements

### Requirement: 原生内嵌 shader

Windows SDL renderer SHALL 从链接到最终 exe 的标准 rc/RCDATA 读取所需 shader，不依赖旁置 shaders、源文件或运行时 shader 编译器。Quad、Glyph、RoundedEffect 的 vertex/fragment 与 DXIL/SPIR-V SHALL 完整，设备格式选择和 GPU ABI SHALL 保持。

#### Scenario: exe 离开 shader 目录
- **WHEN** 将示例 exe 复制到没有 shaders 的目录并从另一 cwd 启动
- **THEN** 实际 GPU pipeline 创建和绘制成功，shader 来自 exe 内嵌资源

### Requirement: 简单构建接入

构建 SHALL 使用现有 shader 输出作为资源输入，并在输入变化后更新最终 exe。Windows SHALL 使用原生 rc 工具，不新增 qrc/XML 清单或框架自制打包协议；其他平台 SHALL 保持固定 shader 集的内嵌读取能力。

#### Scenario: 修改构建输入
- **WHEN** shader 二进制重新生成并构建最终目标
- **THEN** 原生资源中的 bytes 与最新编译输出一致，无需手动复制目录

### Requirement: 默认系统字体 fallback

普通示例 SHALL 默认使用系统字体链，不要求旁置 fonts 或验收字体。显式应用字体 SHALL 保留其顺序和系统 fallback；显式测试/应用 fallback 配置 SHALL 保持。缺失所需字形 SHALL 明确诊断，不伪造系统提供的 coverage。

#### Scenario: 没有旁置字体的示例
- **WHEN** 在具有所需字形的系统上，不配置自定义字体，从无 fonts 的目录启动示例
- **THEN** 实际文字完成布局/绘制，记录系统字体来源，显式锁定字体测试仍可使用

### Requirement: 分层与部署证据

Core 和 renderer/common SHALL 不新增 Windows/SDL 资源加载依赖。验收 SHALL 区分平台通用合同、Windows 原生 rc/GPU/字体与未运行的 Linux 环境，不将构建或历史二进制结果冒充新 exe 的部署成功。

#### Scenario: 平台验收独立记录
- **WHEN** 执行 HEADLESS 检查及原生独立目录启动
- **THEN** 平台隔离保持，记录实际新构建配置、资源内容、backend、字体、退出码及 hashes，未执行的平台保持待验
